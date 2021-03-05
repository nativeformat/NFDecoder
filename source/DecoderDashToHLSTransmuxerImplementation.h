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

#if INCLUDE_UDT

#include <atomic>
#include <memory>
#include <string>

#include <NFDecoder/DataProvider.h>
#include <NFDecoder/DataProviderFactory.h>
#include <NFDecoder/Decrypter.h>
#include <NFDecoder/Factory.h>
#include <NFDecoder/Manifest.h>

#include <DashToHlsApi.h>

#include "DataProviderMemoryImplementation.h"

namespace nativeformat {
namespace decoder {

class DecoderDashToHLSTransmuxerImplementation
    : public Decoder,
      public std::enable_shared_from_this<DecoderDashToHLSTransmuxerImplementation> {
 public:
  typedef std::function<void()> EXHAUST_CALLBACK;

  DecoderDashToHLSTransmuxerImplementation(
      const std::shared_ptr<DataProvider> &data_provider,
      const std::shared_ptr<DataProviderFactory> &data_provider_factory,
      const std::string &path,
      const std::shared_ptr<Factory> &factory,
      const std::shared_ptr<Manifest> &manifest,
      const std::shared_ptr<Decrypter> &decrypter);
  virtual ~DecoderDashToHLSTransmuxerImplementation();

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
  virtual std::string fake_path();

 private:
  void loadSegment(int segment_index,
                   const ERROR_DECODER_CALLBACK &decoder_error_callback,
                   const EXHAUST_CALLBACK &exhaust_callback);
  DashToHlsStatus writeSegment(int segment_index);
  void exhaustDecoder(int segment_index, const EXHAUST_CALLBACK &exhaust_callback);

  static std::atomic<long> _next;
  long _id;

  const std::shared_ptr<DataProvider> _data_provider;
  const std::shared_ptr<DataProviderFactory> _data_provider_factory;
  const std::shared_ptr<Factory> _factory;
  const std::shared_ptr<Manifest> _manifest;
  const std::shared_ptr<Decrypter> _decrypter;

  const std::shared_ptr<DataProviderMemoryImplementation> _data_provider_memory;

  DashToHlsSession *_session;
  DashToHlsIndex *_index;
  std::shared_ptr<Decoder> _decoder;
  std::atomic<long> _frame_index;
  std::vector<float> _samples;
  std::mutex _decoding_mutex;
  long _start_junk_frames;
};

}  // namespace decoder
}  // namespace nativeformat

#endif
