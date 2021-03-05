/*
 * Copyright (c) 2017 Spotify AB.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include "DecoderWavImplementation.h"

#include <cstdlib>
#include <future>

namespace nativeformat {
namespace decoder {

static const char RIFF[] = "RIFF";
static const char WAVE[] = "WAVE";
static const char JUNK[] = "JUNK";
static const char FMT[] = "fmt ";
static const char DATA[] = "data";

static inline bool CHUNK_TYPE(char *s, const char *fcc) {
  std::string str = s;
  return str.compare(0, 4, fcc) == 0;
}

DecoderWavImplementation::DecoderWavImplementation(std::shared_ptr<DataProvider> &data_provider)
    : _data_provider(data_provider),
      _channels(0),
      _samplerate(0.0),
      _frames(0),
      _frame_index(0),
      _data_offset(0) {}

DecoderWavImplementation::~DecoderWavImplementation() {}

const std::string &DecoderWavImplementation::name() {
  static const std::string domain("com.nativeformat.decoder.wav");
  return domain;
}

void DecoderWavImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                    const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  std::shared_ptr<DecoderWavImplementation> strong_this = shared_from_this();
  _load_future = std::async(
      std::launch::async, [strong_this, decoder_error_callback, decoder_load_callback]() {
        // Download the master header
        size_t read_bytes =
            strong_this->_data_provider->read(&strong_this->_header, sizeof(WAVHeader), 1);
        if (read_bytes < sizeof(WAVHeader)) {
          decoder_error_callback(strong_this->name(), ErrorCodeNotEnoughDataForHeader);
          decoder_load_callback(false);
          return;
        } else if (!CHUNK_TYPE(strong_this->_header.riff_header_name, RIFF)) {
          decoder_error_callback(strong_this->name(), ErrorCodeNotRiff);
          decoder_load_callback(false);
        } else if (!CHUNK_TYPE(strong_this->_header.wave_header_name, WAVE)) {
          decoder_error_callback(strong_this->name(), ErrorCodeNotWav);
          decoder_load_callback(false);
        }

        // Find all the chunks we care about, but don't read any data yet
        bool fmt_found = false, data_found = false, ok;
        while (!fmt_found || !data_found) {
          ok = strong_this->readChunk();
          if (!ok) {
            decoder_error_callback(strong_this->name(), ErrorCodeChunkError);
            decoder_load_callback(false);
            return;
          }
          if (CHUNK_TYPE(strong_this->_chunk_type, FMT)) {
            fmt_found = true;
          } else if (CHUNK_TYPE(strong_this->_chunk_type, DATA)) {
            data_found = true;
          }
        }

        // Seek to beginning of data chunk to prepare for decoding
        strong_this->seek(0);
        decoder_load_callback(true);
      });
}

double DecoderWavImplementation::sampleRate() {
  return _samplerate;
}

int DecoderWavImplementation::channels() {
  return _channels;
}

long DecoderWavImplementation::currentFrameIndex() {
  return _frame_index;
}

void DecoderWavImplementation::seek(long frame_index) {
  _data_provider->seek(_data_offset + (frame_index * wavSampleSize(_fmt) * _channels), SEEK_SET);
  _frame_index = frame_index;
}

long DecoderWavImplementation::frames() {
  return _frames;
}

void DecoderWavImplementation::decode(long frames,
                                      const DECODE_CALLBACK &decode_callback,
                                      bool synchronous) {
  long frame_index = _frame_index;
  if (frame_index >= _frames) {
    decode_callback(frame_index, 0, nullptr);
    return;
  }
  std::shared_ptr<DecoderWavImplementation> strong_this = shared_from_this();
  auto run_thread = [strong_this, decode_callback, frames, frame_index] {
    if (frames == 0) {
      decode_callback(frame_index, 0, nullptr);
      return;
    }

    int channels = strong_this->channels();
    if (channels == 0) {
      decode_callback(frame_index, 0, nullptr);
      return;
    }

    size_t sample_size = wavSampleSize(strong_this->_fmt);
    if (sample_size == 0 || strong_this->_fmt.audio_format == WAVHeaderAudioFormatNone) {
      decode_callback(frame_index, 0, nullptr);
      return;
    }

    if (strong_this->_fmt.audio_format == WAVHeaderAudioFormatIEEEFloat) {
      std::vector<float> output(frames * channels);
      size_t bytes_read =
          strong_this->_data_provider->read((void *)output.data(), sample_size * channels, frames);
      size_t frames_read = bytes_read / (sample_size * channels);
      decode_callback(frame_index, frames_read, output.data());
      return;
    }

    // Assume by default that strong_this->_fmt.audio_format ==
    // WAVHeaderAudioFormatPCM
    size_t frames_read = 0;
    switch (sample_size) {
      case 1: {
        WavReader<uint8_t> wv(strong_this->_data_provider.get(), frames, channels);
        frames_read = wv.transferSamples(frames, channels);
        decode_callback(frame_index, frames_read, wv.out_samples.data());
        return;
      }
      case 2: {
        WavReader<int16_t> wv(strong_this->_data_provider.get(), frames, channels);
        frames_read = wv.transferSamples(frames, channels);
        decode_callback(frame_index, frames_read, wv.out_samples.data());
        return;
      }
      case 4: {
        WavReader<int32_t> wv(strong_this->_data_provider.get(), frames, channels);
        frames_read = wv.transferSamples(frames, channels);
        decode_callback(frame_index, frames_read, wv.out_samples.data());
        return;
      }
      default: {
        decode_callback(frame_index, 0, nullptr);
        return;
      }
    }
  };
  if (synchronous) {
    run_thread();
  } else {
    std::thread(run_thread).detach();
  }
}

bool DecoderWavImplementation::eof() {
  return _data_provider->eof();
}

const std::string &DecoderWavImplementation::path() {
  return _data_provider->path();
}

void DecoderWavImplementation::flush() {}

size_t DecoderWavImplementation::wavSampleSize(const FMTHeader &header) {
  return header.bit_depth / 8;
}

bool DecoderWavImplementation::readChunk() {
  if (_data_provider->eof()) return false;

  uint32_t chunk_data_bytes;
  size_t read_bytes = _data_provider->read(_chunk_type, sizeof(char), 4);
  if (read_bytes < 4 && !_data_provider->eof()) {
    fprintf(stderr, "Error %s: Failed to read chunk format\n", name().c_str());
    return false;
  }
  if (CHUNK_TYPE(_chunk_type, FMT)) {
    read_bytes = _data_provider->read(&_fmt, sizeof(FMTHeader), 1);
    if (read_bytes < sizeof(FMTHeader) && !_data_provider->eof()) {
      fprintf(stderr, "Error %s: Failed to read format header\n", name().c_str());
      return false;
    }
    // Parse the information
    uint16_t channels = _fmt.channels;
    _frame_size = (_fmt.bit_depth / 8) * channels;
    _channels = channels;
    _samplerate = static_cast<double>(_fmt.sample_rate);
  } else {
    read_bytes = _data_provider->read(&chunk_data_bytes, sizeof(uint32_t), 1);
    if (read_bytes < 4 && !_data_provider->eof()) {
      fprintf(stderr, "Error %s: Failed to read chunk size\n", name().c_str());
      return false;
    }
    if (CHUNK_TYPE(_chunk_type, DATA)) {
      _data_bytes = chunk_data_bytes;
      _frames = chunk_data_bytes / _frame_size;
      _data_offset = _data_provider->tell();
    }
    if (CHUNK_TYPE(_chunk_type, JUNK) || !knownType()) {
      fprintf(stderr,
              "Warning %s: Skipping unknown chunk type %s with %u bytes\n",
              name().c_str(),
              std::string(_chunk_type, 4).c_str(),
              chunk_data_bytes);
    }
    // Skip data since we're not actually decoding
    if (chunk_data_bytes) {
      _data_provider->seek(chunk_data_bytes, SEEK_CUR);
    }
  }
  return true;
}

bool DecoderWavImplementation::knownType() {
  if (CHUNK_TYPE(_chunk_type, DATA)) return true;
  if (CHUNK_TYPE(_chunk_type, JUNK)) return true;
  if (CHUNK_TYPE(_chunk_type, RIFF)) return true;
  if (CHUNK_TYPE(_chunk_type, WAVE)) return true;
  if (CHUNK_TYPE(_chunk_type, FMT)) return true;
  return false;
}

}  // namespace decoder
}  // namespace nativeformat
