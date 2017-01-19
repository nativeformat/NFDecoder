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

#include <functional>
#include <string>

namespace nativeformat {
namespace decoder {

typedef std::function<void(bool)> LOAD_DATA_PROVIDER_CALLBACK;
typedef std::function<void(const std::string &domain, int error_code)> ERROR_DATA_PROVIDER_CALLBACK;

extern const long UNKNOWN_SIZE;
extern const std::string DATA_PROVIDER_MEMORY_NAME;

class DataProvider {
 public:
  virtual size_t read(void *ptr, size_t size, size_t nmemb) = 0;
  virtual int seek(long offset, int whence) = 0;
  virtual long tell() = 0;
  virtual const std::string &path() = 0;
  virtual bool eof() = 0;
  virtual long size() = 0;
  virtual void load(const ERROR_DATA_PROVIDER_CALLBACK &data_provider_error_callback,
                    const LOAD_DATA_PROVIDER_CALLBACK &data_provider_load_callback) = 0;
  virtual const std::string &name() = 0;
};

}  // namespace decoder
}  // namespace nativeformat
