/*
 * Copyright (c) 2018 Spotify AB.
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
#include "FactoryAndroidImplementation.h"

#include "DecoderAndroidImplementation.h"
#include "Path.h"

#if ANDROID

namespace nativeformat {
namespace decoder {

FactoryAndroidImplementation::FactoryAndroidImplementation(
    std::shared_ptr<Factory> wrapped_factory,
    std::shared_ptr<DataProviderFactory> &data_provider_factory)
    : _wrapped_factory(wrapped_factory), _data_provider_factory(data_provider_factory) {}

FactoryAndroidImplementation::~FactoryAndroidImplementation() {}

void FactoryAndroidImplementation::createDecoder(
    const std::string &path,
    const std::string &mime_type,
    const CREATE_DECODER_CALLBACK create_decoder_callback,
    const ERROR_DECODER_CALLBACK error_decoder_callback,
    double samplerate,
    int channels) {
  auto strong_this = shared_from_this();
  _wrapped_factory->createDecoder(
      path,
      mime_type,
      [create_decoder_callback, strong_this, path, error_decoder_callback](
          std::shared_ptr<Decoder> decoder) {
        if (!decoder) {
          strong_this->_data_provider_factory->createDataProvider(
              path,
              [create_decoder_callback,
               error_decoder_callback](std::shared_ptr<DataProvider> data_provider) {
                if (data_provider == nullptr) {
                  return;
                }
                auto decoder = std::make_shared<DecoderAndroidImplementation>(data_provider);
                decoder->load(error_decoder_callback,
                              [decoder, create_decoder_callback](bool success) {
                                create_decoder_callback(success ? decoder : nullptr);
                              });
              },
              error_decoder_callback);
        } else {
          create_decoder_callback(decoder);
        }
      },
      error_decoder_callback,
      samplerate,
      channels);
}

}  // namespace decoder
}  // namespace nativeformat

#endif
