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
#include "DataProviderHTTPImplementation.h"

#include <sstream>

namespace nativeformat {
namespace decoder {

DataProviderHTTPImplementation::DataProviderHTTPImplementation(const std::string &path,
                                                               std::shared_ptr<http::Client> client)
    : _path(path),
      _client(client ?: http::createClient(http::standardCacheLocation(), "")),
      _content_length(0),
      _offset(0) {}

DataProviderHTTPImplementation::~DataProviderHTTPImplementation() {}

const std::string &DataProviderHTTPImplementation::name() {
  static const std::string domain("com.nativeformat.decoder.http");
  return domain;
}

void DataProviderHTTPImplementation::load(
    const ERROR_DATA_PROVIDER_CALLBACK &data_provider_error_callback,
    const LOAD_DATA_PROVIDER_CALLBACK &data_provider_load_callback) {
  // Perform a HEAD request to check whether the entity is there and get the
  // content length
  std::shared_ptr<http::Request> request = http::createRequest(_path, {});
  request->setMethod(http::HeadMethod);
  std::shared_ptr<DataProviderHTTPImplementation> strong_this = shared_from_this();
  _client->performRequest(
      request,
      [strong_this, data_provider_load_callback, data_provider_error_callback](
          const std::shared_ptr<http::Response> &response) {
        static const std::string content_length_header = "Content-Length";
        if (response->statusCode() != http::StatusCodeOK) {
          data_provider_error_callback(strong_this->name(), response->statusCode());
          data_provider_load_callback(false);
          return;
        }
        std::string content_length = (*response)[content_length_header];
        std::stringstream content_length_stream(content_length);
        size_t content_length_primitive = 0;
        content_length_stream >> content_length_primitive;
        strong_this->_content_length = content_length_primitive;
        strong_this->_load_future = std::async(std::launch::async, [data_provider_load_callback]() {
          data_provider_load_callback(true);
        });
      });
}

size_t DataProviderHTTPImplementation::read(void *ptr, size_t size, size_t nmemb) {
  std::lock_guard<std::mutex> read_lock(_read_mutex);
  if (_offset >= _content_length) {
    return 0;
  }
  size_t offset = _offset;
  size_t new_offset = (offset + (size * nmemb)) - 1;
  std::shared_ptr<http::Request> request = http::createRequest(
      _path, {{"Range", "bytes=" + std::to_string(offset) + "-" + std::to_string(new_offset)}});
  std::shared_ptr<http::Response> response = _client->performRequestSynchronously(request);
  size_t data_length = 0;
  const unsigned char *data = response->data(data_length);
  if (data == nullptr) {
    return 0;
  }
  memcpy(ptr, data, data_length);
  _offset = offset + data_length;
  return data_length;
}

int DataProviderHTTPImplementation::seek(long offset, int whence) {
  std::lock_guard<std::mutex> read_lock(_read_mutex);
  size_t new_offset = 0;
  size_t content_length = _content_length;
  switch (whence) {
    case SEEK_SET:
      new_offset = offset;
      break;
    case SEEK_CUR:
      new_offset = _offset + offset;
      break;
    case SEEK_END:
      new_offset = content_length + offset;
      break;
  }
  if (new_offset > content_length) {
    return EOF;
  }
  _offset = new_offset;
  return 0;
}

long DataProviderHTTPImplementation::tell() {
  std::lock_guard<std::mutex> read_lock(_read_mutex);
  return _offset;
}

const std::string &DataProviderHTTPImplementation::path() {
  return _path;
}

bool DataProviderHTTPImplementation::eof() {
  std::lock_guard<std::mutex> read_lock(_read_mutex);
  return _offset >= _content_length;
}

long DataProviderHTTPImplementation::size() {
  return _content_length;
}

}  // namespace decoder
}  // namespace nativeformat
