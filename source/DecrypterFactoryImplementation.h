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

#include <NFDecoder/DecrypterFactory.h>

#include <memory>

#include "LicenseManager.h"

namespace nativeformat {
namespace decoder {

class DecrypterFactoryImplementation
    : public DecrypterFactory,
      public std::enable_shared_from_this<DecrypterFactoryImplementation> {
 public:
  DecrypterFactoryImplementation(std::shared_ptr<http::Client> client,
                                 std::shared_ptr<ManifestFactory> manifest_factory);
  virtual ~DecrypterFactoryImplementation();

  // DecrypterFactory
  virtual void createDecrypter(
      const std::string &path,
      const CREATE_DECRYPTER_CALLBACK &create_data_provider_callback,
      const Decrypter::ERROR_DECRYPTER_CALLBACK &error_data_provider_callback);

 private:
  const std::shared_ptr<http::Client> _client;
  const std::shared_ptr<ManifestFactory> _manifest_factory;
};

}  // namespace decoder
}  // namespace nativeformat
