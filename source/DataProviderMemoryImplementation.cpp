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
#include "DataProviderMemoryImplementation.h"

namespace nativeformat {
namespace decoder {

DataProviderMemoryImplementation::DataProviderMemoryImplementation(const std::string &path)
    : _path(path) {}

DataProviderMemoryImplementation::~DataProviderMemoryImplementation() {}

const std::string &DataProviderMemoryImplementation::name() {
  return DATA_PROVIDER_MEMORY_NAME;
}

void DataProviderMemoryImplementation::write(void *ptr, size_t size, size_t nmemb) {
  std::lock_guard<std::mutex> lock(_data_mutex);
  unsigned char *data_ptr = (unsigned char *)ptr;
  _data.insert(_data.end(), data_ptr, data_ptr + (size * nmemb));
}

void DataProviderMemoryImplementation::flush() {
  std::lock_guard<std::mutex> lock(_data_mutex);
  _data.clear();
}

void DataProviderMemoryImplementation::load(
    const ERROR_DATA_PROVIDER_CALLBACK &data_provider_error_callback,
    const LOAD_DATA_PROVIDER_CALLBACK &data_provider_load_callback) {
  data_provider_load_callback(true);
}

size_t DataProviderMemoryImplementation::read(void *ptr, size_t size, size_t nmemb) {
  std::lock_guard<std::mutex> lock(_data_mutex);
  size_t read_size = std::min(size * nmemb, _data.size() - (_data.size() % size));
  memcpy(ptr, _data.data(), read_size);
  _data.erase(_data.begin(), _data.begin() + read_size);
  return read_size;
}

int DataProviderMemoryImplementation::seek(long offset, int whence) {
  return -1;
}

long DataProviderMemoryImplementation::tell() {
  return 0;
}

const std::string &DataProviderMemoryImplementation::path() {
  return _path;
}

bool DataProviderMemoryImplementation::eof() {
  std::lock_guard<std::mutex> lock(_data_mutex);
  return _data.empty();
}

long DataProviderMemoryImplementation::size() {
  return -1;
}

}  // namespace decoder
}  // namespace nativeformat
