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
#include <NFDecoder/DecrypterFactory.h>

#include "DecrypterFactoryImplementation.h"

namespace nativeformat {
namespace decoder {

std::shared_ptr<DecrypterFactory> createDecrypterFactory(
    std::shared_ptr<http::Client> client, std::shared_ptr<ManifestFactory> manifest_factory) {
  if (!client) {
    client = http::createClient(http::standardCacheLocation(), "NFDecoder");
  }
  if (!manifest_factory) {
    manifest_factory = createManifestFactory(client);
  }
  return std::make_shared<DecrypterFactoryImplementation>(client, manifest_factory);
}

}  // namespace decoder
}  // namespace nativeformat
