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
#include "Path.h"

namespace nativeformat {
namespace decoder {

bool isMidi(const std::string &path) {
  static const std::string midi_protocol = "midi:";
  return path.size() > midi_protocol.size() &&
         path.substr(0, midi_protocol.size()) == midi_protocol;
}

bool isPathSoundcloud(const std::string &path) {
  static const std::string http_protocol = "http";
  static const std::string https_protocol = "https";
  static const std::string protocol_separator = "://";
  static const std::string soundcloud_domain = "soundcloud.com";
  static const std::string soundcloud_api_domain = "api.soundcloud.com";
  std::string mangled_path = path;
  if (mangled_path.find(http_protocol + protocol_separator) != std::string::npos) {
    auto length = (http_protocol + protocol_separator).length();
    mangled_path = mangled_path.substr(length, mangled_path.length() - length);
  } else if (mangled_path.find(https_protocol + protocol_separator) != std::string::npos) {
    auto length = (https_protocol + protocol_separator).length();
    mangled_path = mangled_path.substr(length, mangled_path.length() - length);
  }
  if ((mangled_path.length() > soundcloud_domain.length() &&
       mangled_path.substr(0, soundcloud_domain.length()) == soundcloud_domain) ||
      (mangled_path.length() > soundcloud_api_domain.length() &&
       mangled_path.substr(0, soundcloud_api_domain.length()) == soundcloud_api_domain)) {
    return true;
  }
  return false;
}

}  // namespace decoder
}  // namespace nativeformat
