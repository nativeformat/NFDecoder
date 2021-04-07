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
#include "FactoryCommonImplementation.h"

#include "DecoderFLACImplementation.h"
#include "DecoderMidiImplementation.h"
#include "DecoderOggImplementation.h"
#include "DecoderSpeexImplementation.h"
#include "DecoderWavImplementation.h"

namespace nativeformat {
namespace decoder {

FactoryCommonImplementation::FactoryCommonImplementation(
    std::shared_ptr<DataProviderFactory> &data_provider_factory)
    : _data_provider_factory(data_provider_factory),
      _extensions_to_types({{NF_DECODER_MIME_TYPE_AUDIO_OGG, std::regex(".*\\.ogg|.*\\.opus")},
                            {NF_DECODER_MIME_TYPE_WAV, std::regex(".*\\.wav")},
                            {NF_DECODER_MIME_TYPE_FLAC, std::regex(".*\\.flac")},
                            {NF_DECODER_MIME_TYPE_MIDI, std::regex("midi\\:.*")},
                            {NF_DECODER_MIME_TYPE_SPEEX, std::regex(".*\\.spx")}}) {}

FactoryCommonImplementation::~FactoryCommonImplementation() {}

void FactoryCommonImplementation::createDecoder(
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
  if (NF_DECODER_OGG_MIME_TYPES.find(mime_type_check) != NF_DECODER_OGG_MIME_TYPES.end()) {
    _data_provider_factory->createDataProvider(
        path,
        [create_decoder_callback,
         error_decoder_callback](std::shared_ptr<DataProvider> data_provider) {
          createDecoder<DecoderOggImplementation>(
              data_provider, create_decoder_callback, error_decoder_callback);
        },
        error_decoder_callback);
    return;
  } else if (NF_DECODER_WAV_MIME_TYPES.find(mime_type_check) != NF_DECODER_WAV_MIME_TYPES.end()) {
    _data_provider_factory->createDataProvider(
        path,
        [create_decoder_callback,
         error_decoder_callback](std::shared_ptr<DataProvider> data_provider) {
          createDecoder<DecoderWavImplementation>(
              data_provider, create_decoder_callback, error_decoder_callback);
        },
        error_decoder_callback);
    return;
  } else if (NF_DECODER_FLAC_MIME_TYPES.find(mime_type_check) != NF_DECODER_FLAC_MIME_TYPES.end()) {
    _data_provider_factory->createDataProvider(
        path,
        [create_decoder_callback,
         error_decoder_callback](std::shared_ptr<DataProvider> data_provider) {
          createDecoder<DecoderFLACImplementation>(
              data_provider, create_decoder_callback, error_decoder_callback);
        },
        error_decoder_callback);
    return;
  } else if (NF_DECODER_MIDI_MIME_TYPES.find(mime_type_check) != NF_DECODER_MIDI_MIME_TYPES.end()) {
    auto decoder = std::make_shared<DecoderMidiImplementation>(path);
    decoder->load(error_decoder_callback, [decoder, create_decoder_callback](bool success) {
      create_decoder_callback(success ? decoder : nullptr);
    });
    return;
  } else if (NF_DECODER_SPEEX_MIME_TYPES.find(mime_type_check) !=
             NF_DECODER_SPEEX_MIME_TYPES.end()) {
    _data_provider_factory->createDataProvider(
        path,
        [create_decoder_callback,
         error_decoder_callback](std::shared_ptr<DataProvider> data_provider) {
          createDecoder<DecoderSpeexImplementation>(
              data_provider, create_decoder_callback, error_decoder_callback);
        },
        error_decoder_callback);
    return;
  }
  create_decoder_callback(nullptr);
}

template <typename DecoderType>
void FactoryCommonImplementation::createDecoder(
    std::shared_ptr<DataProvider> data_provider,
    const CREATE_DECODER_CALLBACK create_decoder_callback,
    const ERROR_DECODER_CALLBACK error_decoder_callback) {
  if (!data_provider) {
    create_decoder_callback(nullptr);
    return;
  }
  std::shared_ptr<DecoderType> decoder = std::make_shared<DecoderType>(data_provider);
  decoder->load(error_decoder_callback, [decoder, create_decoder_callback](bool success) {
    create_decoder_callback(success ? decoder : nullptr);
  });
}

}  // namespace decoder
}  // namespace nativeformat
