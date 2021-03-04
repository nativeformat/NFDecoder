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

#include <NFDecoder/Decoder.h>

#include <atomic>
#include <future>
#include <mutex>
#include <vector>

#include <NFDecoder/DataProvider.h>
#include <NFDecoder/Factory.h>

#include <libresample.h>

namespace nativeformat {
namespace decoder {

class DecoderNormalisationImplementation
    : public Decoder,
      public std::enable_shared_from_this<DecoderNormalisationImplementation> {
 public:
  typedef enum : int { ErrorCodeCouldNotDecodeHeader } ErrorCode;

  DecoderNormalisationImplementation(const std::shared_ptr<Decoder> &wrapped_decoder,
                                     const double samplerate,
                                     const int channels);
  virtual ~DecoderNormalisationImplementation();

  // Decoder
  virtual double sampleRate();
  virtual int channels();
  virtual long currentFrameIndex();
  virtual void seek(long frame_index);
  virtual long frames();
  virtual void decode(long frames, const DECODE_CALLBACK &decode_callback, bool synchronous);
  virtual bool eof();
  virtual const std::string &path();
  virtual const std::string &name();
  virtual void flush();
  virtual void load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                    const LOAD_DECODER_CALLBACK &decoder_load_callback);

 private:
  const std::shared_ptr<Decoder> _wrapped_decoder;

  double _factor;
  void *_resampler_handlers[2];
  std::atomic<long> _frame_index;
  std::mutex _resampler_mutex;
  std::vector<float> _pcm_buffer;
  std::atomic<double> _samplerate;
  std::atomic_int _channels;
};

}  // namespace decoder
}  // namespace nativeformat
