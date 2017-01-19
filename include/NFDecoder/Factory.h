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
#include <memory>
#include <string>

#include <NFDecoder/DataProviderFactory.h>
#include <NFDecoder/Decoder.h>
#include <NFDecoder/DecrypterFactory.h>
#include <NFDecoder/ManifestFactory.h>
#include <NFDecoder/NFDecoderMimeTypes.h>

namespace nativeformat {
namespace decoder {

typedef std::function<void(std::shared_ptr<Decoder> decoder)> CREATE_DECODER_CALLBACK;

extern const double STANDARD_SAMPLERATE;
extern const int STANDARD_CHANNELS;

class Factory {
 public:
  virtual void createDecoder(const std::string &path,
                             const std::string &mime_type,
                             const CREATE_DECODER_CALLBACK create_decoder_callback,
                             const ERROR_DECODER_CALLBACK error_decoder_callback,
                             double samplerate = STANDARD_SAMPLERATE,
                             int channels = STANDARD_CHANNELS) = 0;
};

extern std::shared_ptr<Factory> createFactory(
    std::shared_ptr<DataProviderFactory> data_provider_factory = nullptr,
    std::shared_ptr<DecrypterFactory> decrypter_factory = nullptr,
    std::shared_ptr<ManifestFactory> manifest_factory = nullptr);

}  // namespace decoder
}  // namespace nativeformat
