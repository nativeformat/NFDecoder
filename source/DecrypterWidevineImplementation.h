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

#include <NFDecoder/Decrypter.h>

#include <memory>

#include <NFHTTP/Client.h>

#include "LicenseManager.h"
#include "WidevineCDMSessionManager.h"

namespace nativeformat {
namespace decoder {

class DecrypterWidevineImplementation
    : public Decrypter,
      public WidevineCDMSessionManagerDelegate,
      public std::enable_shared_from_this<DecrypterWidevineImplementation> {
 public:
  DecrypterWidevineImplementation(const std::shared_ptr<WidevineCDMSessionManager> &session_manager,
                                  const std::shared_ptr<LicenseManager> &license_manager,
                                  const std::shared_ptr<http::Client> &client,
                                  const std::string &pssh);
  virtual ~DecrypterWidevineImplementation();

  static const std::string &domain();

  // Decrypter
  virtual int decrypt(const std::vector<unsigned char> &input,
                      std::vector<unsigned char> &output,
                      const unsigned char *key_id,
                      int key_id_length,
                      const unsigned char *iv,
                      int iv_length);
  virtual void load(LOAD_DECRYPTER_CALLBACK load_decrypter_callback,
                    ERROR_DECRYPTER_CALLBACK error_decrypter_callback);

  // WidevineCDMSessionManagerDelegate
  virtual void onMessage(widevine::Cdm::MessageType message_type, const std::string &message);
  virtual void onKeyStatusChange();
  virtual void onRemoveComplete();

 private:
  const std::shared_ptr<WidevineCDMSessionManager> _session_manager;
  const std::shared_ptr<LicenseManager> _license_manager;
  const std::shared_ptr<http::Client> _client;
  const std::string _pssh;

  std::string _session_id;
  LOAD_DECRYPTER_CALLBACK _load_callback;
  ERROR_DECRYPTER_CALLBACK _error_callback;
};

}  // namespace decoder
}  // namespace nativeformat
