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
#include "DataProviderFactoryImplementation.h"

#include <nlohmann/json.hpp>

#include "DataProviderFileImplementation.h"
#include "DataProviderHTTPImplementation.h"
#include "Path.h"

namespace nativeformat {
namespace decoder {

std::atomic<int> DataProviderFactoryImplementation::_creator_count{0};

DataProviderFactoryImplementation::DataProviderFactoryImplementation(
    std::shared_ptr<http::Client> client, std::shared_ptr<ManifestFactory> manifest_factory)
    : _http_client(client), _manifest_factory(manifest_factory) {}

DataProviderFactoryImplementation::~DataProviderFactoryImplementation() {}

std::string DataProviderFactoryImplementation::domain() {
  static const std::string domain("com.nativeformat.dataprovider.factory");
  return domain;
}

void DataProviderFactoryImplementation::createDataProvider(
    const std::string &path,
    const CREATE_DATA_PROVIDER_CALLBACK &create_data_provider_callback,
    const ERROR_DATA_PROVIDER_CALLBACK &error_data_provider_callback) {
  static const std::string http_protocol = "http://";
  static const std::string https_protocol = "https://";
  std::shared_ptr<DataProvider> data_provider = nullptr;
  {
    std::lock_guard<std::mutex> lock(_creator_mutex);
    for (auto it : _creator_functions) {
      data_provider = it.second(path);
      if (data_provider) {
        break;
      }
    }
  }
  if (!data_provider) {
    if ((path.size() >= http_protocol.size() &&
         path.substr(0, http_protocol.size()) == http_protocol) ||
        (path.size() >= https_protocol.size() &&
         path.substr(0, https_protocol.size()) == https_protocol)) {
      if (isPathSoundcloud(path) && path.find("/stream") == std::string::npos) {
        // Resolve the streaming URL
        auto weak_this = std::weak_ptr<DataProviderFactoryImplementation>(shared_from_this());
        auto soundcloud_resolve_request =
            http::createRequest("https://api.soundcloud.com/resolve?url=" + path, {});
        _http_client->performRequest(
            soundcloud_resolve_request,
            [create_data_provider_callback, error_data_provider_callback, weak_this](
                const std::shared_ptr<http::Response> &response) {
              if (auto strong_this = weak_this.lock()) {
                if (response->statusCode() == http::StatusCodeOK) {
                  size_t data_length = 0;
                  auto data = response->data(data_length);
                  auto json = nlohmann::json::parse(std::string((const char *)data, data_length));
                  auto stream_url = json["stream_url"].get<std::string>();
                  strong_this->createDataProvider(
                      stream_url, create_data_provider_callback, error_data_provider_callback);
                } else {
                  create_data_provider_callback(nullptr);
                  error_data_provider_callback(strong_this->domain(), response->statusCode());
                }
              }
            });
        return;
      } else {
        data_provider = std::make_shared<DataProviderHTTPImplementation>(path, _http_client);
      }
    } else {
      data_provider = std::make_shared<DataProviderFileImplementation>(path);
    }
  }
  // If we tried everything and there is still no data_provider, error...
  if (!data_provider) {
    create_data_provider_callback(nullptr);
    return;
  }
  data_provider->load(error_data_provider_callback,
                      [data_provider, create_data_provider_callback](bool success) {
                        create_data_provider_callback(success ? data_provider : nullptr);
                      });
}

int DataProviderFactoryImplementation::addDataProviderCreator(
    const DATA_PROVIDER_CREATOR_FUNCTION &data_provider_creator_function) {
  int creator_index = _creator_count++;
  std::lock_guard<std::mutex> lock(_creator_mutex);
  _creator_functions[creator_index] = data_provider_creator_function;
  return creator_index;
}

void DataProviderFactoryImplementation::removeDataProviderCreator(int creator_index) {
  std::lock_guard<std::mutex> lock(_creator_mutex);
  _creator_functions.erase(creator_index);
}

}  // namespace decoder
}  // namespace nativeformat
