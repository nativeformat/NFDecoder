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
#include "DecoderFLACImplementation.h"

#include <cstdlib>
#include <future>

namespace nativeformat {
namespace decoder {

DecoderFLACImplementation::DecoderFLACImplementation(std::shared_ptr<DataProvider> &data_provider)
    : _data_provider(data_provider),
      _flac_decoder(nullptr),
      _channels(0),
      _samplerate(0.0),
      _frame_index(0),
      _frames(0) {}

DecoderFLACImplementation::~DecoderFLACImplementation() {
  if (_flac_decoder != nullptr) {
    FLAC__stream_decoder_delete(_flac_decoder);
  }
}

const std::string &DecoderFLACImplementation::name() {
  static const std::string domain("com.nativeformat.decoder.flac");
  return domain;
}

void DecoderFLACImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                     const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  FLAC__StreamDecoderInitStatus init_status = FLAC__STREAM_DECODER_INIT_STATUS_OK;
  {
    std::lock_guard<std::mutex> flac_decoder_lock(_flac_decoder_mutex);
    _flac_decoder = FLAC__stream_decoder_new();
    FLAC__stream_decoder_set_md5_checking(_flac_decoder, true);
    init_status = FLAC__stream_decoder_init_stream(_flac_decoder,
                                                   &DecoderFLACImplementation::flac_read,
                                                   &DecoderFLACImplementation::flac_seek,
                                                   &DecoderFLACImplementation::flac_tell,
                                                   &DecoderFLACImplementation::flac_length,
                                                   &DecoderFLACImplementation::flac_eof,
                                                   &DecoderFLACImplementation::flac_write,
                                                   &DecoderFLACImplementation::flac_metadata,
                                                   &DecoderFLACImplementation::flac_error,
                                                   this);
  }

  if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    decoder_error_callback(name(), init_status);
    decoder_load_callback(false);
    return;
  }

  auto strong_this = shared_from_this();
  _load_future = std::async(
      std::launch::async, [strong_this, decoder_error_callback, decoder_load_callback]() {
        FLAC__bool success = false;
        {
          std::lock_guard<std::mutex> flac_decoder_lock(strong_this->_flac_decoder_mutex);
          success = FLAC__stream_decoder_process_until_end_of_metadata(strong_this->_flac_decoder);
        }
        if (!success) {
          FLAC__StreamDecoderState error_code = FLAC__STREAM_DECODER_SEARCH_FOR_METADATA;
          {
            std::lock_guard<std::mutex> flac_decoder_lock(strong_this->_flac_decoder_mutex);
            error_code = FLAC__stream_decoder_get_state(strong_this->_flac_decoder);
          }
          decoder_error_callback(strong_this->name(), error_code);
          decoder_load_callback(false);
          return;
        }
        decoder_load_callback(true);
      });
}

double DecoderFLACImplementation::sampleRate() {
  return _samplerate;
}

int DecoderFLACImplementation::channels() {
  return _channels;
}

long DecoderFLACImplementation::currentFrameIndex() {
  return _frame_index;
}

void DecoderFLACImplementation::seek(long frame_index) {
  std::lock_guard<std::mutex> flac_decoder_lock(_flac_decoder_mutex);
  std::lock_guard<std::mutex> samples_lock(_samples_mutex);
  FLAC__stream_decoder_seek_absolute(_flac_decoder, frame_index);
  _samples.clear();
  _frame_index = frame_index;
}

long DecoderFLACImplementation::frames() {
  return _frames;
}

void DecoderFLACImplementation::decode(long frames,
                                       const DECODE_CALLBACK &decode_callback,
                                       bool synchronous) {
  auto strong_this = shared_from_this();
  auto run_thread = [decode_callback, strong_this, frames]() {
    std::lock_guard<std::mutex> flac_decoder_lock(strong_this->_flac_decoder_mutex);
    auto channels = strong_this->channels();
    auto frame_index = strong_this->currentFrameIndex();
    long frame_count = 0;
    {
      std::lock_guard<std::mutex> samples_lock(strong_this->_samples_mutex);
      frame_count = strong_this->_samples.size() / channels;
    }
    while (frame_count < frames) {
      if (!FLAC__stream_decoder_process_single(strong_this->_flac_decoder)) {
        break;
      }
      {
        std::lock_guard<std::mutex> samples_lock(strong_this->_samples_mutex);
        frame_count = strong_this->_samples.size() / channels;
      }
    }
    auto read_frames = std::min(frame_count, frames);
    {
      std::lock_guard<std::mutex> samples_lock(strong_this->_samples_mutex);
      decode_callback(frame_index, read_frames, &strong_this->_samples[0]);
      auto read_samples = read_frames * channels;
      strong_this->_samples.erase(strong_this->_samples.begin(),
                                  strong_this->_samples.begin() + read_samples);
      strong_this->_frame_index = frame_index + read_frames;
    }
  };
  if (synchronous) {
    run_thread();
  } else {
    std::thread(run_thread).detach();
  }
}

bool DecoderFLACImplementation::eof() {
  return _data_provider->eof();
}

const std::string &DecoderFLACImplementation::path() {
  return _data_provider->path();
}

void DecoderFLACImplementation::flush() {
  std::lock_guard<std::mutex> flac_decoder_lock(_flac_decoder_mutex);
  FLAC__stream_decoder_flush(_flac_decoder);
  _samples.clear();
}

FLAC__StreamDecoderReadStatus DecoderFLACImplementation::flac_read(
    const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data) {
  DecoderFLACImplementation *flac_decoder = (DecoderFLACImplementation *)client_data;
  *bytes = flac_decoder->_data_provider->read(buffer, *bytes, 1);
  return flac_decoder->_data_provider->eof() ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM
                                             : FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderSeekStatus DecoderFLACImplementation::flac_seek(
    const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data) {
  DecoderFLACImplementation *flac_decoder = (DecoderFLACImplementation *)client_data;
  flac_decoder->_data_provider->seek(absolute_byte_offset, SEEK_SET);
  return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus DecoderFLACImplementation::flac_tell(
    const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data) {
  DecoderFLACImplementation *flac_decoder = (DecoderFLACImplementation *)client_data;
  *absolute_byte_offset = flac_decoder->_data_provider->tell();
  return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus DecoderFLACImplementation::flac_length(
    const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data) {
  DecoderFLACImplementation *flac_decoder = (DecoderFLACImplementation *)client_data;
  *stream_length = flac_decoder->_data_provider->size();
  return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool DecoderFLACImplementation::flac_eof(const FLAC__StreamDecoder *decoder,
                                               void *client_data) {
  DecoderFLACImplementation *flac_decoder = (DecoderFLACImplementation *)client_data;
  return flac_decoder->_data_provider->eof();
}

FLAC__StreamDecoderWriteStatus DecoderFLACImplementation::flac_write(
    const FLAC__StreamDecoder *decoder,
    const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[],
    void *client_data) {
  DecoderFLACImplementation *flac_decoder = (DecoderFLACImplementation *)client_data;
  int channels = flac_decoder->channels();
  auto frames = frame->header.blocksize;
  auto sample_count = frames * channels;
  float *samples = (float *)malloc(sample_count * sizeof(float));
  float sample_denominator = (float)std::numeric_limits<FLAC__int16>::max();
  for (uint32_t i = 0; i < frame->header.blocksize; ++i) {
    for (int channel = 0; channel < channels; ++channel) {
      FLAC__int16 sample = (FLAC__int16)buffer[channel][i];
      samples[(i * channels) + channel] = (float)sample / sample_denominator;
    }
  }
  {
    std::lock_guard<std::mutex> samples_lock(flac_decoder->_samples_mutex);
    flac_decoder->_samples.insert(flac_decoder->_samples.end(), samples, samples + sample_count);
  }
  free(samples);
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void DecoderFLACImplementation::flac_metadata(const FLAC__StreamDecoder *decoder,
                                              const FLAC__StreamMetadata *metadata,
                                              void *client_data) {
  DecoderFLACImplementation *flac_decoder = (DecoderFLACImplementation *)client_data;
  flac_decoder->_samplerate = metadata->data.stream_info.sample_rate;
  flac_decoder->_channels = metadata->data.stream_info.channels;
  flac_decoder->_frames = metadata->data.stream_info.total_samples;
}

void DecoderFLACImplementation::flac_error(const FLAC__StreamDecoder *decoder,
                                           FLAC__StreamDecoderErrorStatus status,
                                           void *client_data) {
  // TODO: Implement if we need
}

}  // namespace decoder
}  // namespace nativeformat
