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
#include <NFDecoder/NFDecoderMimeTypes.h>

namespace nativeformat {
namespace decoder {

const std::string NF_DECODER_MIME_TYPE_MP2TS("mp2ts");
const std::string NF_DECODER_MIME_TYPE_VIDEO_MP2TS("video/mp2ts");
const std::string NF_DECODER_MIME_TYPE_AUDIO_MP2TS("audio/mp2ts");
const std::set<std::string> NF_DECODER_MPEG2TS_MIME_TYPES({NF_DECODER_MIME_TYPE_MP2TS,
                                                           NF_DECODER_MIME_TYPE_VIDEO_MP2TS,
                                                           NF_DECODER_MIME_TYPE_AUDIO_MP2TS});

const std::string NF_DECODER_MIME_TYPE_OGG("ogg");
const std::string NF_DECODER_MIME_TYPE_AUDIO_OGG("audio/ogg");
const std::string NF_DECODER_MIME_TYPE_APPLICATION_OGG("application/ogg");
const std::set<std::string> NF_DECODER_OGG_MIME_TYPES({NF_DECODER_MIME_TYPE_OGG,
                                                       NF_DECODER_MIME_TYPE_AUDIO_OGG,
                                                       NF_DECODER_MIME_TYPE_APPLICATION_OGG});

const std::string NF_DECODER_MIME_TYPE_WAV("audio/wav");
const std::set<std::string> NF_DECODER_WAV_MIME_TYPES(
    {NF_DECODER_MIME_TYPE_WAV, "audio/x-wav", "audio/wave", "audio/x-pn-wave"});

const std::string NF_DECODER_MIME_TYPE_FLAC("flac");
const std::string NF_DECODER_MIME_TYPE_AUDIO_FLAC("audio/flac");
const std::set<std::string> NF_DECODER_FLAC_MIME_TYPES({NF_DECODER_MIME_TYPE_FLAC,
                                                        NF_DECODER_MIME_TYPE_AUDIO_FLAC});

const std::string NF_DECODER_MIME_TYPE_DASH_MP4("dash/mp4");
const std::set<std::string> NF_DECODER_DASH_MP4_MIME_TYPES({NF_DECODER_MIME_TYPE_DASH_MP4});

const std::string NF_DECODER_MIME_TYPE_MP3("audio/mpeg");
const std::set<std::string> NF_DECODER_MP3_MIME_TYPES({NF_DECODER_MIME_TYPE_MP3});

const std::string NF_DECODER_MIME_TYPE_MIDI("midi");
const std::set<std::string> NF_DECODER_MIDI_MIME_TYPES({NF_DECODER_MIME_TYPE_MIDI});

const std::string NF_DECODER_MIME_TYPE_SPEEX_OGG("audio/x-speex");
extern const std::string NF_DECODER_MIME_TYPE_SPEEX("audio/speex");
extern const std::set<std::string> NF_DECODER_SPEEX_MIME_TYPES({NF_DECODER_MIME_TYPE_SPEEX_OGG,
                                                                NF_DECODER_MIME_TYPE_SPEEX});

}  // namespace decoder
}  // namespace nativeformat
