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
#include "DecrypterWidevineImplementation.h"

namespace nativeformat {
namespace decoder {

DecrypterWidevineImplementation::DecrypterWidevineImplementation(
    const std::shared_ptr<WidevineCDMSessionManager> &session_manager,
    const std::shared_ptr<LicenseManager> &license_manager,
    const std::shared_ptr<http::Client> &client,
    const std::string &pssh)
    : _session_manager(session_manager),
      _license_manager(license_manager),
      _client(client),
      _pssh(pssh) {}

DecrypterWidevineImplementation::~DecrypterWidevineImplementation() {}

const std::string &DecrypterWidevineImplementation::domain() {
  static const std::string domain("com.nativeformat.decoder.decrypter.widevine");
  return domain;
}

int DecrypterWidevineImplementation::decrypt(const std::vector<unsigned char> &input,
                                             std::vector<unsigned char> &output,
                                             const unsigned char *key_id,
                                             int key_id_length,
                                             const unsigned char *iv,
                                             int iv_length) {
  if (input.empty()) {
    return DECRYPTER_SUCCESS;
  }

  // create input buffer
  widevine::Cdm::InputBuffer input_buffer;
  input_buffer.key_id = key_id;
  input_buffer.key_id_length = key_id_length;
  input_buffer.iv = iv;
  input_buffer.iv_length = iv_length;
  input_buffer.data = input.data();
  input_buffer.data_length = input.size();
  input_buffer.is_encrypted = true;
  input_buffer.block_offset = 0;

  // create output buffer
  widevine::Cdm::OutputBuffer output_buffer;
  output_buffer.data_offset = 0;
  output_buffer.data_length = output.size();
  output_buffer.data = output.data();
  output_buffer.is_secure = false;

  return _session_manager->decrypt(input_buffer, output_buffer);

  return 0;
}

void DecrypterWidevineImplementation::load(LOAD_DECRYPTER_CALLBACK load_decrypter_callback,
                                           ERROR_DECRYPTER_CALLBACK error_decrypter_callback) {
  auto status = _session_manager->createSession(shared_from_this(), _session_id);
  if (status != widevine::Cdm::kSuccess) {
    load_decrypter_callback(false);
    error_decrypter_callback(domain(), status);
    return;
  }
  _load_callback = load_decrypter_callback;
  _error_callback = error_decrypter_callback;
  _session_manager->generateRequest(_session_id, widevine::Cdm::kCenc, _pssh);
}

void DecrypterWidevineImplementation::onMessage(widevine::Cdm::MessageType message_type,
                                                const std::string &message) {
  auto weak_this = std::weak_ptr<DecrypterWidevineImplementation>(shared_from_this());
  _license_manager->loadLicenseURL([weak_this, message](const std::string &license_url,
                                                        const std::string &domain,
                                                        int error_code) {
    if (auto strong_this = weak_this.lock()) {
      if (error_code != LICENSE_MANAGER_SUCCESS) {
        strong_this->_error_callback(strong_this->domain(), error_code);
        strong_this->_load_callback(false);
        return;
      }
      auto request = http::createRequest(license_url, {});
      request->setData((const unsigned char *)message.c_str(), message.length());
      request->setMethod(http::PostMethod);
      strong_this->_client->performRequest(
          request, [weak_this](const std::shared_ptr<http::Response> &response) {
            if (auto strong_this = weak_this.lock()) {
              auto status = response->statusCode();
              if (status != http::StatusCodeOK) {
                strong_this->_error_callback(strong_this->domain(), status);
                strong_this->_load_callback(false);
                return;
              }
              size_t data_length = 0;
              const unsigned char *data = response->data(data_length);
              std::string response_string = std::string((const char *)data, data_length);
              widevine::Cdm::Status widevine_status =
                  strong_this->_session_manager->update(strong_this->_session_id, response_string);
              if (widevine_status != widevine::Cdm::kSuccess) {
                strong_this->_error_callback(strong_this->domain(), widevine_status);
                strong_this->_load_callback(false);
                return;
              }
              strong_this->_load_callback(true);
            }
          });
    }
  });
}

void DecrypterWidevineImplementation::onKeyStatusChange() {}

void DecrypterWidevineImplementation::onRemoveComplete() {}

}  // namespace decoder
}  // namespace nativeformat
