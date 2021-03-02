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
#include "FactoryNormalisationImplementation.h"

#include "DecoderNormalisationImplementation.h"

namespace nativeformat {
namespace decoder {

FactoryNormalisationImplementation::FactoryNormalisationImplementation(
    std::shared_ptr<Factory> wrapped_factory)
    : _wrapped_factory(wrapped_factory) {}

FactoryNormalisationImplementation::~FactoryNormalisationImplementation() {}

void FactoryNormalisationImplementation::createDecoder(
    const std::string &path,
    const std::string &mime_type,
    const CREATE_DECODER_CALLBACK create_decoder_callback,
    const ERROR_DECODER_CALLBACK error_decoder_callback,
    double samplerate,
    int channels) {
  _wrapped_factory->createDecoder(
      path,
      mime_type,
      [create_decoder_callback, error_decoder_callback, samplerate, channels](
          std::shared_ptr<Decoder> decoder) {
        if (!decoder) {
          create_decoder_callback(decoder);
          return;
        }
        // No point in normalising an already normalised decoder
        if (decoder->sampleRate() == samplerate && decoder->channels() == channels) {
          create_decoder_callback(decoder);
          return;
        }
        auto normalised_decoder =
            std::make_shared<DecoderNormalisationImplementation>(decoder, samplerate, channels);
        normalised_decoder->load(error_decoder_callback,
                                 [create_decoder_callback, normalised_decoder](bool success) {
                                   create_decoder_callback(normalised_decoder);
                                 });
      },
      error_decoder_callback);
}

}  // namespace decoder
}  // namespace nativeformat
