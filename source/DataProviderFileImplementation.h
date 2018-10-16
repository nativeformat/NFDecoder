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

#include <NFDecoder/DataProvider.h>
#include <NFDecoder/DataProviderFactory.h>

#include <atomic>
#include <string>

namespace nativeformat {
namespace decoder {

typedef std::function<void(bool)> LOAD_DATA_PROVIDER_CALLBACK;

class DataProviderFileImplementation : public DataProvider {
 public:
  typedef enum : int { ErrorCodeCouldNotReadFile } ErrorCode;

  DataProviderFileImplementation(const std::string &path);
  virtual ~DataProviderFileImplementation();

  // DataProvider
  virtual size_t read(void *ptr, size_t size, size_t nmemb);
  virtual int seek(long offset, int whence);
  virtual long tell();
  virtual const std::string &path();
  virtual bool eof();
  virtual long size();
  virtual void load(const ERROR_DATA_PROVIDER_CALLBACK &data_provider_error_callback,
                    const LOAD_DATA_PROVIDER_CALLBACK &data_provider_load_callback);
  virtual const std::string &name();

 private:
  const std::string _path;

  FILE *_handle;
  std::atomic<long> _size;
};

}  // namespace decoder
}  // namespace nativeformat
