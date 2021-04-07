/*
 * Copyright (c) 2021 Spotify AB.
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
#include "DecoderSpeexImplementation.h"

#include <cstdlib>
#include <future>

#include <speex/speex_header.h>

namespace nativeformat {
namespace decoder {

DecoderSpeexImplementation::DecoderSpeexImplementation(std::shared_ptr<DataProvider> &data_provider)
    : _data_provider(data_provider),
      _state(nullptr),
      _channels(1),
      _samplerate(0.0),
      _frames(0),
      _frame_index(0),
      _current_section(0) {}

DecoderSpeexImplementation::~DecoderSpeexImplementation() {
  if (_state != nullptr) {
    speex_decoder_destroy(_state);
    speex_bits_destroy(&_bits);
    _state = nullptr;
  }
}

const std::string &DecoderSpeexImplementation::name() {
  static const std::string domain("com.nativeformat.decoder.speex");
  return domain;
}

void DecoderSpeexImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                      const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  std::shared_ptr<DecoderSpeexImplementation> strong_this = shared_from_this();
  _load_future = std::async(std::launch::async,
                            [strong_this, decoder_error_callback, decoder_load_callback]() {
                              {
                                std::lock_guard<std::mutex> speex_lock(strong_this->_speex_mutex);
                                strong_this->_state = speex_decoder_init(&speex_nb_mode);
                                int tmp = 1;
                                speex_decoder_ctl(strong_this->_state, SPEEX_SET_ENH, &tmp);
                                speex_bits_init(&strong_this->_bits);
                                SpeexHeader header;
                                strong_this->_data_provider->read(&header, sizeof(SpeexHeader), 1);
                                strong_this->_samplerate = header.rate;
                                strong_this->_channels = header.nb_channels;
                              }
                              decoder_load_callback(true);
                            });
}

double DecoderSpeexImplementation::sampleRate() {
  return _samplerate;
}

int DecoderSpeexImplementation::channels() {
  return _channels;
}

long DecoderSpeexImplementation::currentFrameIndex() {
  return _frame_index;
}

void DecoderSpeexImplementation::seek(long frame_index) {
  flush();
  {
    std::lock_guard<std::mutex> speex_lock(_speex_mutex);
    _data_provider->seek(sizeof(SpeexHeader), SEEK_SET);
    int frame_size = 0;
    speex_decoder_ctl(_state, SPEEX_GET_FRAME_SIZE, &frame_size);
    long current_frame_index = 0;
    while (!_data_provider->eof() && current_frame_index < frame_index) {
      _cached_samples.clear();
      char read_bytes[256];
      const auto bytes_read = _data_provider->read(read_bytes, sizeof(char), sizeof(read_bytes));
      speex_bits_read_from(&_bits, read_bytes, bytes_read);
      float samples[frame_size];
      const auto samples_read = speex_decode(_state, &_bits, samples);
      _cached_samples.insert(_cached_samples.begin(), samples, samples + samples_read);
      current_frame_index += samples_read;
    }
    _frame_index = frame_index;
  }
}

long DecoderSpeexImplementation::frames() {
  return UNKNOWN_FRAMES;
}

void DecoderSpeexImplementation::decode(long frames,
                                        const DECODE_CALLBACK &decode_callback,
                                        bool synchronous) {
  std::shared_ptr<DecoderSpeexImplementation> strong_this = shared_from_this();
  auto run_thread = [strong_this, decode_callback, frames] {
    long frame_index = strong_this->currentFrameIndex();
    float *samples = (float *)malloc(frames * sizeof(float) * strong_this->_channels);
    long read_frames = 0;
    {
      std::lock_guard<std::mutex> speex_lock(strong_this->_speex_mutex);
      int frame_size = 0;
      speex_decoder_ctl(strong_this->_state, SPEEX_GET_FRAME_SIZE, &frame_size);
      while (!strong_this->_data_provider->eof() && strong_this->_cached_samples.size() < frames) {
        char read_bytes[256];
        const auto bytes_read =
            strong_this->_data_provider->read(read_bytes, sizeof(char), sizeof(read_bytes));
        speex_bits_read_from(&strong_this->_bits, read_bytes, bytes_read);
        float samples[frame_size];
        const auto samples_read = speex_decode(strong_this->_state, &strong_this->_bits, samples);
        strong_this->_cached_samples.insert(
            strong_this->_cached_samples.end(), samples, samples + samples_read);
      }
    }
    read_frames = std::min(static_cast<long>(strong_this->_cached_samples.size()), frames);
    memcpy(samples, strong_this->_cached_samples.data(), read_frames * sizeof(float));
    strong_this->_cached_samples.erase(strong_this->_cached_samples.begin(),
                                       strong_this->_cached_samples.begin() + read_frames);
    decode_callback(frame_index, read_frames, samples);
    free(samples);
  };
  if (synchronous) {
    run_thread();
  } else {
    std::thread(run_thread).detach();
  }
}

bool DecoderSpeexImplementation::eof() {
  return _data_provider->eof();
}

const std::string &DecoderSpeexImplementation::path() {
  return _data_provider->path();
}

void DecoderSpeexImplementation::flush() {
  std::lock_guard<std::mutex> speex_lock(_speex_mutex);
  speex_decoder_ctl(_state, SPEEX_RESET_STATE, nullptr);
  speex_bits_reset(&_bits);
  _cached_samples.clear();
}

}  // namespace decoder
}  // namespace nativeformat
