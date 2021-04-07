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

#include <set>
#include <string>

namespace nativeformat {
namespace decoder {

// MPEG2TS
extern const std::string NF_DECODER_MIME_TYPE_MP2TS;
extern const std::string NF_DECODER_MIME_TYPE_VIDEO_MP2TS;
extern const std::string NF_DECODER_MIME_TYPE_AUDIO_MP2TS;
extern const std::set<std::string> NF_DECODER_MPEG2TS_MIME_TYPES;

// OGG
extern const std::string NF_DECODER_MIME_TYPE_OGG;
extern const std::string NF_DECODER_MIME_TYPE_AUDIO_OGG;
extern const std::string NF_DECODER_MIME_TYPE_APPLICATION_OGG;
extern const std::set<std::string> NF_DECODER_OGG_MIME_TYPES;

// WAV
extern const std::string NF_DECODER_MIME_TYPE_WAV;
extern const std::set<std::string> NF_DECODER_WAV_MIME_TYPES;

// FLAC
extern const std::string NF_DECODER_MIME_TYPE_FLAC;
extern const std::string NF_DECODER_MIME_TYPE_AUDIO_FLAC;
extern const std::set<std::string> NF_DECODER_FLAC_MIME_TYPES;

// DASH
extern const std::string NF_DECODER_MIME_TYPE_DASH_MP4;
extern const std::set<std::string> NF_DECODER_DASH_MP4_MIME_TYPES;

// MP3
extern const std::string NF_DECODER_MIME_TYPE_MP3;
extern const std::set<std::string> NF_DECODER_MP3_MIME_TYPES;

// MIDI
extern const std::string NF_DECODER_MIME_TYPE_MIDI;
extern const std::set<std::string> NF_DECODER_MIDI_MIME_TYPES;

// SPEEX
extern const std::string NF_DECODER_MIME_TYPE_SPEEX_OGG;
extern const std::string NF_DECODER_MIME_TYPE_SPEEX;
extern const std::set<std::string> NF_DECODER_SPEEX_MIME_TYPES;

}  // namespace decoder
}  // namespace nativeformat
