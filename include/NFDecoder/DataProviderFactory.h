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
#include <memory>
#include <string>

#include <NFHTTP/Client.h>

#include <NFDecoder/DataProvider.h>
#include <NFDecoder/ManifestFactory.h>

namespace nativeformat {
namespace decoder {

class DataProviderFactory {
 public:
  typedef std::function<void(std::shared_ptr<DataProvider> data_provider)>
      CREATE_DATA_PROVIDER_CALLBACK;
  typedef std::function<std::shared_ptr<DataProvider>(const std::string &path)>
      DATA_PROVIDER_CREATOR_FUNCTION;

  virtual void createDataProvider(
      const std::string &path,
      const CREATE_DATA_PROVIDER_CALLBACK &create_data_provider_callback,
      const ERROR_DATA_PROVIDER_CALLBACK &error_data_provider_callback) = 0;
  virtual int addDataProviderCreator(
      const DATA_PROVIDER_CREATOR_FUNCTION &data_provider_creator_function) = 0;
  virtual void removeDataProviderCreator(int creator_index) = 0;
};

extern std::shared_ptr<DataProviderFactory> createDataProviderFactory(
    std::shared_ptr<http::Client> client = nullptr,
    std::shared_ptr<ManifestFactory> manifest_factory = nullptr);

}  // namespace decoder
}  // namespace nativeformat
