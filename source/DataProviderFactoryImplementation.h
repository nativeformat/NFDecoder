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

#include <NFDecoder/DataProviderFactory.h>

#include <memory>
#include <string>

namespace nativeformat {
namespace decoder {

class DataProviderFactoryImplementation
    : public DataProviderFactory,
      public std::enable_shared_from_this<DataProviderFactoryImplementation> {
 public:
  DataProviderFactoryImplementation(std::shared_ptr<http::Client> client,
                                    std::shared_ptr<ManifestFactory> manifest_factory);
  virtual ~DataProviderFactoryImplementation();

  static std::string domain();

  // DataProviderFactory
  virtual void createDataProvider(
      const std::string &path,
      const CREATE_DATA_PROVIDER_CALLBACK &create_data_provider_callback,
      const ERROR_DATA_PROVIDER_CALLBACK &error_data_provider_callback);
  virtual int addDataProviderCreator(
      const DATA_PROVIDER_CREATOR_FUNCTION &data_provider_creator_function);
  virtual void removeDataProviderCreator(int creator_index);

 private:
  const std::shared_ptr<http::Client> _http_client;
  const std::shared_ptr<ManifestFactory> _manifest_factory;

  std::mutex _creator_mutex;
  std::map<int, DATA_PROVIDER_CREATOR_FUNCTION> _creator_functions;
  static std::atomic<int> _creator_count;
};

}  // namespace decoder
}  // namespace nativeformat
