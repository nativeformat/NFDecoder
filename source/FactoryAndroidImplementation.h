/*
 * Copyright (c) 2018 Spotify AB.
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

#include <NFDecoder/Factory.h>

#if ANDROID

#include <memory>

namespace nativeformat {
namespace decoder {

class FactoryAndroidImplementation
    : public Factory,
      public std::enable_shared_from_this<FactoryAndroidImplementation> {
 public:
  FactoryAndroidImplementation(std::shared_ptr<Factory> wrapped_factory,
                               std::shared_ptr<DataProviderFactory> &data_provider_factory);
  virtual ~FactoryAndroidImplementation();

  // Factory
  virtual void createDecoder(const std::string &path,
                             const std::string &mime_type,
                             const CREATE_DECODER_CALLBACK create_decoder_callback,
                             const ERROR_DECODER_CALLBACK error_decoder_callback,
                             double samplerate,
                             int channels);

 private:
  std::shared_ptr<Factory> _wrapped_factory;
  std::shared_ptr<DataProviderFactory> _data_provider_factory;
};

}  // namespace decoder
}  // namespace nativeformat

#endif
