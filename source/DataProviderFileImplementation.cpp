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
#include "DataProviderFileImplementation.h"

namespace nativeformat {
namespace decoder {

DataProviderFileImplementation::DataProviderFileImplementation(const std::string &path)
    : _path(path) {}

DataProviderFileImplementation::~DataProviderFileImplementation() {}

const std::string &DataProviderFileImplementation::name() {
  static const std::string domain("com.nativeformat.decoder.file");
  return domain;
}

void DataProviderFileImplementation::load(
    const ERROR_DATA_PROVIDER_CALLBACK &data_provider_error_callback,
    const LOAD_DATA_PROVIDER_CALLBACK &data_provider_load_callback) {
  _handle = fopen(path().c_str(), "r");
  if (!_handle) {
    printf("Failed to open file: %s\n", path().c_str());
    data_provider_error_callback(name(), ErrorCodeCouldNotReadFile);
    data_provider_load_callback(false);
  } else {
    fseek(_handle, 0, SEEK_END);
    _size = ftell(_handle);
    fseek(_handle, 0, SEEK_SET);
    data_provider_load_callback(true);
  }
}

size_t DataProviderFileImplementation::read(void *ptr, size_t size, size_t nmemb) {
  size_t data_read = fread(ptr, size, nmemb, _handle);
  return data_read * size;
}

int DataProviderFileImplementation::seek(long offset, int whence) {
  return fseek(_handle, offset, whence);
}

long DataProviderFileImplementation::tell() {
  return ftell(_handle);
}

const std::string &DataProviderFileImplementation::path() {
  return _path;
}

bool DataProviderFileImplementation::eof() {
  return !!feof(_handle);
}

long DataProviderFileImplementation::size() {
  return _size;
}

}  // namespace decoder
}  // namespace nativeformat
