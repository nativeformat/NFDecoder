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
#pragma once

#include <functional>
#include <string>

namespace nativeformat {
namespace decoder {

typedef std::function<void(long frame_index, long frame_count, float *samples)> DECODE_CALLBACK;
typedef std::function<void(bool success)> LOAD_DECODER_CALLBACK;
typedef std::function<void(const std::string &domain, int error_code)> ERROR_DECODER_CALLBACK;

extern const long UNKNOWN_FRAMES;
extern const std::string DECODER_AUDIOCONVERTER_NAME;

extern const std::string version();

class Decoder {
 public:
  virtual double sampleRate() = 0;
  virtual int channels() = 0;
  virtual long currentFrameIndex() = 0;
  virtual void seek(long frame_index) = 0;
  virtual long frames() = 0;
  virtual void decode(long frames,
                      const DECODE_CALLBACK &decode_callback,
                      bool synchronous = false) = 0;
  virtual bool eof() = 0;
  virtual const std::string &path() = 0;
  virtual const std::string &name() = 0;
  virtual void flush() = 0;
  virtual void load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                    const LOAD_DECODER_CALLBACK &decoder_load_callback) = 0;
};

}  // namespace decoder
}  // namespace nativeformat
