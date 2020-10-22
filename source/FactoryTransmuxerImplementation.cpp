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
#include "FactoryTransmuxerImplementation.h"

#include <NFDecoder/NFDecoderMimeTypes.h>

#include "DecoderDashToHLSTransmuxerImplementation.h"
#include "Path.h"

namespace nativeformat {
namespace decoder {

namespace {
static const char DASH_FILE_INDICATOR[] = "ftypdash\0";
static const long DASH_FILE_INDICATOR_OFFSET = 4;
}  // namespace

FactoryTransmuxerImplementation::FactoryTransmuxerImplementation(
    std::shared_ptr<Factory> wrapped_factory,
    std::shared_ptr<DataProviderFactory> &data_provider_factory,
    std::shared_ptr<ManifestFactory> &manifest_factory,
    std::shared_ptr<DecrypterFactory> &decrypter_factory)
    : _wrapped_factory(wrapped_factory),
      _data_provider_factory(data_provider_factory),
      _manifest_factory(manifest_factory),
      _decrypter_factory(decrypter_factory),
      _extensions_to_types({{NF_DECODER_MIME_TYPE_DASH_MP4, std::regex(".*\\.mp4")}}) {}

FactoryTransmuxerImplementation::~FactoryTransmuxerImplementation() {}

void FactoryTransmuxerImplementation::createDecoder(
    const std::string &path,
    const std::string &mime_type,
    const CREATE_DECODER_CALLBACK create_decoder_callback,
    const ERROR_DECODER_CALLBACK error_decoder_callback,
    double samplerate,
    int channels) {
  std::string mime_type_check = mime_type;
  if (mime_type_check.empty()) {
    for (auto it = _extensions_to_types.begin(); it != _extensions_to_types.end(); ++it) {
      auto &ext = it->second;
      if (std::regex_match(path, ext)) {
        mime_type_check = it->first;
        break;
      }
    }
  }
  bool should_process =
#if USE_FFMPEG
      false;
#else
      (NF_DECODER_DASH_MP4_MIME_TYPES.find(mime_type_check) !=
       NF_DECODER_DASH_MP4_MIME_TYPES.end());
#endif

  if (should_process) {
    auto strong_this = shared_from_this();
    _data_provider_factory->createDataProvider(
        path,
        [strong_this, path, error_decoder_callback, create_decoder_callback, mime_type](
            const std::shared_ptr<DataProvider> &data_provider) {
          if (!data_provider) {
            return;
          }
          data_provider->seek(DASH_FILE_INDICATOR_OFFSET, SEEK_SET);
          char FILE_INDICATOR[sizeof(DASH_FILE_INDICATOR)];
          data_provider->read(&FILE_INDICATOR, sizeof(char), sizeof(FILE_INDICATOR));
          std::string str = DASH_FILE_INDICATOR;
          bool is_dash_file = str.compare(FILE_INDICATOR) == 0;
          data_provider->seek(0, SEEK_SET);

          if (!is_dash_file) {
            strong_this->_wrapped_factory->createDecoder(
                path,
                mime_type,
                [create_decoder_callback, error_decoder_callback](
                    std::shared_ptr<Decoder> decoder) { create_decoder_callback(decoder); },
                error_decoder_callback);
            return;
          }
        },
        error_decoder_callback);
    return;
  }
  _wrapped_factory->createDecoder(
      path,
      mime_type,
      [create_decoder_callback, error_decoder_callback](std::shared_ptr<Decoder> decoder) {
        create_decoder_callback(decoder);
      },
      error_decoder_callback);
}

}  // namespace decoder
}  // namespace nativeformat
