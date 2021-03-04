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
#include "DecoderOpusImplementation.h"

#include <cstdlib>
#include <future>

namespace nativeformat {
namespace decoder {

static const int OPUS_READ_SIZE = 32768;  // Read 32kB at a time

const OpusFileCallbacks DecoderOpusImplementation::callbacks{
    &DecoderOpusImplementation::opus_read,
    &DecoderOpusImplementation::opus_seek,
    &DecoderOpusImplementation::opus_tell,
    &DecoderOpusImplementation::opus_close};

DecoderOpusImplementation::DecoderOpusImplementation(std::shared_ptr<DataProvider> &data_provider)
    : _data_provider(data_provider),
      _opus_file(nullptr),
      _channels(0),
      _samplerate(0.0),
      _frames(0),
      _frame_index(0),
      _current_section(0) {}

DecoderOpusImplementation::~DecoderOpusImplementation() {
  if (_opus_file) {
    op_free(_opus_file);
  }
}

const std::string &DecoderOpusImplementation::name() {
  static const std::string domain("com.nativeformat.decoder.opus");
  return domain;
}

bool DecoderOpusImplementation::checkCodec() {
  std::lock_guard<std::mutex> opus_lock(_opus_mutex);
  int error_code;
  _opus_file = op_test_callbacks(this, &callbacks, nullptr, 0, &error_code);
  if (error_code) {
    return false;
  }
  error_code = op_test_open(_opus_file);
  if (error_code) {
    printf("Could not open opus file: %s\n", opus_error(error_code).c_str());
    return false;
  }
  op_set_read_size(_opus_file, OPUS_READ_SIZE);
  return true;
}

void DecoderOpusImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                     const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  std::shared_ptr<DecoderOpusImplementation> strong_this = shared_from_this();
  _load_future = std::async(
      std::launch::async, [strong_this, decoder_error_callback, decoder_load_callback]() {
        {
          std::lock_guard<std::mutex> opus_lock(strong_this->_opus_mutex);

          // Open file if checkCodec was not previously called
          if (!strong_this->_opus_file) {
            int error_code = 0;
            strong_this->_opus_file = op_open_callbacks(
                strong_this.get(), &strong_this->callbacks, nullptr, 0, &error_code);
            if (error_code) {
              printf("Could not open opus file: %s\n", opus_error(error_code).c_str());
              decoder_error_callback(strong_this->name(), ErrorCodeCouldNotDecode);
              decoder_load_callback(false);
              return;
            }
            op_set_read_size(strong_this->_opus_file, OPUS_READ_SIZE);
          }

          int channels = op_channel_count(strong_this->_opus_file, -1);
          long long frames = op_pcm_total(strong_this->_opus_file, -1);

          if (channels < 0 || frames < 0) {
            decoder_error_callback(strong_this->name(), std::min((long long)channels, frames));
            decoder_load_callback(false);
          }

          strong_this->_channels = channels;
          strong_this->_samplerate = 48000.0;  // all opus audio is 48 KHz
          strong_this->_frames = frames;
        }
        decoder_load_callback(true);
      });
}

double DecoderOpusImplementation::sampleRate() {
  return _samplerate;
}

int DecoderOpusImplementation::channels() {
  return _channels;
}

long DecoderOpusImplementation::currentFrameIndex() {
  return _frame_index;
}

void DecoderOpusImplementation::seek(long frame_index) {
  std::lock_guard<std::mutex> opus_lock(_opus_mutex);
  int error_code = op_pcm_seek(_opus_file, frame_index);
  if (error_code != 0) {
    printf("Seek failed: %s\n", opus_error(error_code).c_str());
    return;
  }
  _frame_index = frame_index;
}

long DecoderOpusImplementation::frames() {
  return _frames;
}

void DecoderOpusImplementation::decode(long frames,
                                       const DECODE_CALLBACK &decode_callback,
                                       bool synchronous) {
  std::shared_ptr<DecoderOpusImplementation> strong_this = shared_from_this();
  auto run_thread = [strong_this, decode_callback, frames] {
    long frame_index = strong_this->currentFrameIndex();

    // Make sure opus file is on the same page
    strong_this->seek(frame_index);

    float *samples = (float *)malloc(frames * sizeof(float) * strong_this->_channels);
    long read_frames = 0, read_samples = 0;
    {
      std::lock_guard<std::mutex> opus_lock(strong_this->_opus_mutex);
      while (read_frames < frames) {
        long current_read_frames = op_read_float(strong_this->_opus_file,
                                                 samples + read_samples,
                                                 frames - read_frames,
                                                 &strong_this->_current_section);
        if (current_read_frames <= 0) {
          if (current_read_frames == OP_HOLE) {
            continue;
          }
          if (current_read_frames < 0) {
            printf("Decode error: %s\n", opus_error(current_read_frames).c_str());
          }
          break;
        }
        // update channel count in case we moved to a new section
        int channels = op_channel_count(strong_this->_opus_file, strong_this->_current_section);
        if (channels > strong_this->_channels) {
          samples = (float *)realloc(samples, frames * sizeof(float) * channels);
        }
        strong_this->_channels = channels;
        read_frames += current_read_frames;
        read_samples += current_read_frames * channels;
      }
      strong_this->_frame_index = frame_index + read_frames;
    }
    decode_callback(frame_index, read_frames, samples);
    free(samples);
  };
  if (synchronous) {
    run_thread();
  } else {
    std::thread(run_thread).detach();
  }
}

bool DecoderOpusImplementation::eof() {
  return _data_provider->eof();
}

const std::string &DecoderOpusImplementation::path() {
  return _data_provider->path();
}

void DecoderOpusImplementation::flush() {}

int DecoderOpusImplementation::opus_read(void *datasource, unsigned char *ptr, int nbytes) {
  DecoderOpusImplementation *decoder = (DecoderOpusImplementation *)datasource;
  return decoder->_data_provider->read(ptr, 1, nbytes);
}

int DecoderOpusImplementation::opus_seek(void *datasource, ogg_int64_t offset, int whence) {
  DecoderOpusImplementation *decoder = (DecoderOpusImplementation *)datasource;
  int s = decoder->_data_provider->seek(offset, whence);
  return s;
}

int DecoderOpusImplementation::opus_close(void *datasource) {
  return 0;
}

int64_t DecoderOpusImplementation::opus_tell(void *datasource) {
  DecoderOpusImplementation *decoder = (DecoderOpusImplementation *)datasource;
  return decoder->_data_provider->tell();
}

std::string DecoderOpusImplementation::opus_error(int code) {
  switch (code) {
    case 0:
      return "No error";
    case OP_ENOTFORMAT:
      return "Stream is not an opus file";
    default:
      return "Other opus error";
  }
}

}  // namespace decoder
}  // namespace nativeformat
