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
#include "FactoryServiceImplementation.h"

#include "Path.h"

namespace nativeformat {
namespace decoder {

FactoryServiceImplementation::FactoryServiceImplementation(
    std::shared_ptr<Factory> wrapped_factory,
    std::shared_ptr<DataProviderFactory> &data_provider_factory,
    std::shared_ptr<ManifestFactory> &manifest_factory,
    std::shared_ptr<DecrypterFactory> &decrypter_factory)
    : _wrapped_factory(wrapped_factory),
      _data_provider_factory(data_provider_factory),
      _manifest_factory(manifest_factory),
      _decrypter_factory(decrypter_factory) {}

FactoryServiceImplementation::~FactoryServiceImplementation() {}

void FactoryServiceImplementation::createDecoder(
    const std::string &path,
    const std::string &mime_type,
    const CREATE_DECODER_CALLBACK create_decoder_callback,
    const ERROR_DECODER_CALLBACK error_decoder_callback,
    double samplerate,
    int channels) {
  std::string altered_mime_type = mime_type;
  if (isPathSoundcloud(path)) {
    altered_mime_type = NF_DECODER_MIME_TYPE_MP3;
  }
  _wrapped_factory->createDecoder(
      path,
      altered_mime_type,
      [create_decoder_callback, error_decoder_callback](std::shared_ptr<Decoder> decoder) {
        create_decoder_callback(decoder);
      },
      error_decoder_callback,
      samplerate,
      channels);
}

}  // namespace decoder
}  // namespace nativeformat
