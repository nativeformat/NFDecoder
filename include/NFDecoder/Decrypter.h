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

#include <functional>
#include <vector>

namespace nativeformat {
namespace decoder {

extern const int DECRYPTER_SUCCESS;

class Decrypter {
 public:
  typedef std::function<void(bool success)> LOAD_DECRYPTER_CALLBACK;
  typedef std::function<void(const std::string &domain, int error_code)> ERROR_DECRYPTER_CALLBACK;

  virtual int decrypt(const std::vector<unsigned char> &input,
                      std::vector<unsigned char> &output,
                      const unsigned char *key_id,
                      int key_id_length,
                      const unsigned char *iv,
                      int iv_length) = 0;
  virtual void load(LOAD_DECRYPTER_CALLBACK load_decrypter_callback,
                    ERROR_DECRYPTER_CALLBACK error_decrypter_callback) = 0;
};

}  // namespace decoder
}  // namespace nativeformat
