/*
 * Copyright (c) 2016 Spotify AB.
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
#pragma once

#include <cstddef>
#include <memory>
#include <set>
#include <string>

#include <NFDecoder/NFDecoderMimeTypes.h>

namespace nativeformat {
namespace decoder {

typedef std::function<void(std::string domain, int error)> NF_DECODER_ERROR_CALLBACK;

extern const double NF_DECODER_SAMPLE_RATE_INVALID;
extern const size_t NF_DECODER_CHANNELS_INVALID;

/**
 * This is the generic interface to a decoder for obtaining the PCM samples
 */
class NFDecoder {
 public:
  virtual size_t write(const unsigned char *data, size_t data_size) = 0;
  virtual size_t read(float *samples, size_t number_of_samples) = 0;
  virtual size_t samples() = 0;
  virtual std::string mimeType() = 0;
  virtual size_t channels() = 0;
  virtual double sampleRate() = 0;
  virtual void flush() = 0;
};

extern std::shared_ptr<NFDecoder> decoderForData(
    const unsigned char *data,
    size_t data_size,
    const std::string &mime_type,
    NF_DECODER_ERROR_CALLBACK error_callback,
    double sample_rate = NF_DECODER_SAMPLE_RATE_INVALID,
    size_t channels = NF_DECODER_CHANNELS_INVALID);

}  // namespace decoder
}  // namespace nativeformat
