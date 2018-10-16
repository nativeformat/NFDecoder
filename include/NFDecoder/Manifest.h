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

#include <nlohmann/json.hpp>

namespace nativeformat {
namespace decoder {

class Manifest {
 public:
  typedef std::function<void(bool success)> LOAD_MANIFEST_CALLBACK;
  typedef std::function<void(const std::string &domain, int error_code)> ERROR_MANIFEST_CALLBACK;

  virtual nlohmann::json json() = 0;
  virtual void load(LOAD_MANIFEST_CALLBACK load_manifest_callback,
                    ERROR_MANIFEST_CALLBACK error_manifest_callback) = 0;
};

}  // namespace decoder
}  // namespace nativeformat
