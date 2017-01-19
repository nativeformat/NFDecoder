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
#include <NFDecoder/Factory.h>

#include "FactoryAndroidImplementation.h"
#include "FactoryAppleImplementation.h"
#include "FactoryCommonImplementation.h"
#include "FactoryLGPLImplementation.h"
#include "FactoryNormalisationImplementation.h"
#include "FactoryServiceImplementation.h"
#include "FactoryTransmuxerImplementation.h"

namespace nativeformat {
namespace decoder {

const double STANDARD_SAMPLERATE = 44100.0;
const int STANDARD_CHANNELS = 2;

std::shared_ptr<Factory> createCommonFactory(
    std::shared_ptr<DataProviderFactory> data_provider_factory,
    std::shared_ptr<DecrypterFactory> decrypter_factory,
    std::shared_ptr<ManifestFactory> manifest_factory) {
  return std::make_shared<FactoryCommonImplementation>(data_provider_factory);
}

std::shared_ptr<Factory> createPlatformFactory(
    std::shared_ptr<DataProviderFactory> data_provider_factory,
    std::shared_ptr<DecrypterFactory> decrypter_factory,
    std::shared_ptr<ManifestFactory> manifest_factory) {
  std::shared_ptr<Factory> common_factory =
      createCommonFactory(data_provider_factory, decrypter_factory, manifest_factory);
#if __APPLE__ && !USE_FFMPEG
  return std::make_shared<FactoryAppleImplementation>(common_factory, data_provider_factory);
#endif
#if ANDROID
  return std::make_shared<FactoryAndroidImplementation>(common_factory, data_provider_factory);
#endif
  return common_factory;
}

std::shared_ptr<Factory> createLGPLFactory(
    std::shared_ptr<DataProviderFactory> data_provider_factory,
    std::shared_ptr<DecrypterFactory> decrypter_factory,
    std::shared_ptr<ManifestFactory> manifest_factory) {
  std::shared_ptr<Factory> platform_factory =
      createPlatformFactory(data_provider_factory, decrypter_factory, manifest_factory);
#if INCLUDE_LGPL
  return std::make_shared<FactoryLGPLImplementation>(
      platform_factory, data_provider_factory, decrypter_factory);
#endif
  return platform_factory;
}

std::shared_ptr<Factory> createTransmuxerFactory(
    std::shared_ptr<DataProviderFactory> data_provider_factory,
    std::shared_ptr<DecrypterFactory> decrypter_factory,
    std::shared_ptr<ManifestFactory> manifest_factory) {
  auto lgpl_factory = createLGPLFactory(data_provider_factory, decrypter_factory, manifest_factory);
  return std::make_shared<FactoryTransmuxerImplementation>(
      lgpl_factory, data_provider_factory, manifest_factory, decrypter_factory);
}

std::shared_ptr<Factory> createNormalisationFactory(
    std::shared_ptr<DataProviderFactory> data_provider_factory,
    std::shared_ptr<DecrypterFactory> decrypter_factory,
    std::shared_ptr<ManifestFactory> manifest_factory) {
  return std::make_shared<FactoryNormalisationImplementation>(
      createTransmuxerFactory(data_provider_factory, decrypter_factory, manifest_factory));
}

std::shared_ptr<Factory> createServiceFactory(
    std::shared_ptr<DataProviderFactory> data_provider_factory,
    std::shared_ptr<DecrypterFactory> decrypter_factory,
    std::shared_ptr<ManifestFactory> manifest_factory) {
  return std::make_shared<FactoryServiceImplementation>(
      createNormalisationFactory(data_provider_factory, decrypter_factory, manifest_factory),
      data_provider_factory,
      manifest_factory,
      decrypter_factory);
}

std::shared_ptr<Factory> createFactory(std::shared_ptr<DataProviderFactory> data_provider_factory,
                                       std::shared_ptr<DecrypterFactory> decrypter_factory,
                                       std::shared_ptr<ManifestFactory> manifest_factory) {
  if (!data_provider_factory) {
    data_provider_factory = createDataProviderFactory();
  }
  if (!decrypter_factory) {
    decrypter_factory = createDecrypterFactory();
  }
  if (!manifest_factory) {
    manifest_factory = createManifestFactory();
  }
  return createServiceFactory(data_provider_factory, decrypter_factory, manifest_factory);
}

}  // namespace decoder
}  // namespace nativeformat
