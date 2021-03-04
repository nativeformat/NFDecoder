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
#include "DecoderOggImplementation.h"
#include "DecoderOpusImplementation.h"
#include "DecoderVorbisImplementation.h"

#include <cstdlib>
#include <future>

namespace nativeformat {
namespace decoder {

DecoderOggImplementation::DecoderOggImplementation(std::shared_ptr<DataProvider> &data_provider)
    : _data_provider(data_provider), _decoder(nullptr) {}

DecoderOggImplementation::~DecoderOggImplementation() {}

const std::string &DecoderOggImplementation::name() {
  static const std::string domain("com.nativeformat.decoder.ogg");
  return domain;
}

void DecoderOggImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                    const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  // Try both vorbis and opus decoders
  auto vorbisDecoder = std::make_shared<DecoderVorbisImplementation>(_data_provider);
  if (vorbisDecoder.get()->checkCodec()) {
    _decoder = vorbisDecoder;
    _decoder->load(decoder_error_callback, decoder_load_callback);
    return;
  }

  _data_provider->seek(0, SEEK_SET);
  auto opusDecoder = std::make_shared<DecoderOpusImplementation>(_data_provider);
  if (opusDecoder.get()->checkCodec()) {
    _decoder = opusDecoder;
    _decoder->load(decoder_error_callback, decoder_load_callback);
    return;
  }

  decoder_error_callback(name(), ErrorCodeCouldNotDecode);
  decoder_load_callback(false);
}

double DecoderOggImplementation::sampleRate() {
  return _decoder->sampleRate();
}

int DecoderOggImplementation::channels() {
  return _decoder->channels();
}

long DecoderOggImplementation::currentFrameIndex() {
  return _decoder->currentFrameIndex();
}

void DecoderOggImplementation::seek(long frame_index) {
  _decoder->seek(frame_index);
}

long DecoderOggImplementation::frames() {
  return _decoder->frames();
}

void DecoderOggImplementation::decode(long frames,
                                      const DECODE_CALLBACK &decode_callback,
                                      bool synchronous) {
  _decoder->decode(frames, decode_callback, synchronous);
}

bool DecoderOggImplementation::eof() {
  return _decoder->eof();
}

const std::string &DecoderOggImplementation::path() {
  return _data_provider->path();
}

void DecoderOggImplementation::flush() {
  _decoder->flush();
}

}  // namespace decoder
}  // namespace nativeformat
