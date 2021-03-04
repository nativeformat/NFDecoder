/*
 * Copyright (c) 2016 Spotify AB.
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

#include <cstddef>
#include <fstream>
#include <iostream>

#include <NFHTTP/Client.h>
#include <NFHTTP/Request.h>

#include <nlohmann/json.hpp>

#include <cstdlib>

#include <unistd.h>

static std::string https_protocol = "https://";
static std::string soundcloud_domain = "api.soundcloud.com";
static std::string authorization_header = "Authorization";
static std::string client_id_query_key = "client_id";
static std::string http_query_begin = "?";
static std::string http_query_separator = "&";
static std::string http_query_key_value_separator = "=";

static const char RIFFHeaderValue[] = "RIFF";
static const char WAVHeaderValue[] = "WAVE";
static const char FMTHeaderValue[] = "fmt ";
static const char DATAHeaderValue[] = "data";

typedef enum : short {
  WAVHeaderAudioFormatPCM = 1,
  WAVHeaderAudioFormatIEEEFloat = 3
} WAVHeaderAudioFormat;

#pragma pack(push, 1)
typedef struct WAVHeader {
  // RIFF
  char riff_header_name[4];
  int file_size;
  char wave_header_name[4];
  // Format
  char fmt_header_name[4];
  int chunk_size;
  WAVHeaderAudioFormat audio_format;
  short channels;
  int sample_rate;
  int byte_rate;
  short sample_alignment;
  short bit_depth;
  // Data
  char data_header[4];
  int data_bytes;
} WAVHeader;
#pragma pack(pop)

int main(int argc, char *argv[]) {
  std::cout << "NFDecoder Command Line Interface " << nativeformat::decoder::version() << std::endl;

  if (argc < 3 || argc > 5) {
    std::cerr << "Invalid number of arguments: ./NFDecoderCLI [input] [output] [offset] [duration]"
              << std::endl;
    std::exit(1);
  }

  // Add Token Handling
  const auto user_agent = "NFDecoder-" + nativeformat::decoder::version();
  auto token_client =
      nativeformat::http::createClient(nativeformat::http::standardCacheLocation(), user_agent);
  auto client = nativeformat::http::createClient(
      nativeformat::http::standardCacheLocation(),
      user_agent,
      [token_client](
          std::function<void(const std::shared_ptr<nativeformat::http::Request> &request)> callback,
          const std::shared_ptr<nativeformat::http::Request> &request) {
        // Is it HTTPS?
        if (request->url().length() > https_protocol.length() &&
            request->url().substr(0, https_protocol.length()) == https_protocol) {
          std::string remaining_url = request->url().substr(https_protocol.length());
          // is it a soundcloud domain?
          if (remaining_url.length() > soundcloud_domain.length() &&
              remaining_url.substr(0, soundcloud_domain.length()) == soundcloud_domain) {
            if (remaining_url.find(client_id_query_key + http_query_key_value_separator) ==
                std::string::npos) {
              auto new_request = nativeformat::http::createRequest(request);
              // API Key must be in URL passed in
              new_request->setUrl(new_request->url());
              callback(new_request);
              return;
            }
          }
        }
        callback(request);
      });

  const std::string media_location = argv[1];
  const std::string media_output = argv[2];
  const float offset = argc > 3 ? std::stof(argv[3]) : 0;
  const float render_duration = argc > 4 ? std::stof(argv[4]) : -1.0f;

  std::cout << "Input File: " << media_location << std::endl;
  std::cout << "Output File: " << media_output << std::endl;
  if (offset) std::cout << "Offset: " << offset << " seconds" << std::endl;

  std::fstream raw_handle(media_output, std::ios::out | std::ios::binary);

  if (raw_handle.fail()) {
    std::cout << "Failed to open output file" << std::endl;
    std::exit(1);
  }

  auto manifest_factory = nativeformat::decoder::createManifestFactory(client);
  auto data_provider_factory =
      nativeformat::decoder::createDataProviderFactory(client, manifest_factory);
  auto decrypter_factory = nativeformat::decoder::createDecrypterFactory(client, manifest_factory);
  std::shared_ptr<nativeformat::decoder::Factory> factory = nativeformat::decoder::createFactory(
      data_provider_factory, decrypter_factory, manifest_factory);
  factory->createDecoder(
      media_location,
      "",
      [&raw_handle, offset, render_duration](
          std::shared_ptr<nativeformat::decoder::Decoder> decoder) {
        std::cout << "Decoder created with " << decoder->frames() << " frames "
                  << decoder->channels() << " channels " << decoder->sampleRate() << " sample rate"
                  << std::endl;
        std::size_t frame_index = offset * decoder->sampleRate();
        if (offset) decoder->seek(frame_index);
        long decode_frames = 0;
        if (render_duration < 0.0f) {
          if (decoder->frames() == nativeformat::decoder::UNKNOWN_FRAMES) {
            decode_frames = decoder->sampleRate() * 30;
          } else {
            decode_frames = decoder->frames() - frame_index;
          }
        } else {
          decode_frames = render_duration * decoder->sampleRate();
        }
        std::cout << "Decoding " << decode_frames << " frames" << std::endl;
        decoder->decode(
            decode_frames,
            [decoder, &raw_handle](long frame_index, long frame_count, float *samples_ptr) {
              std::cout << "Decoded " << frame_count << " frames" << std::endl;
              if (samples_ptr != nullptr) {
                int channels = decoder->channels();
                int samples = frame_count * channels;
                int sample_rate = decoder->sampleRate();
                // Write out the raw PCM
                static const std::size_t bits_per_byte = 8;
                std::size_t byte_size = samples * sizeof(float);
                WAVHeader header = {
                    // RIFF Header
                    {
                        RIFFHeaderValue[0],
                        RIFFHeaderValue[1],
                        RIFFHeaderValue[2],
                        RIFFHeaderValue[3],
                    },
                    static_cast<int>(byte_size + sizeof(WAVHeader) - (sizeof(char) * 4) -
                                     sizeof(int)),
                    {WAVHeaderValue[0], WAVHeaderValue[1], WAVHeaderValue[2], WAVHeaderValue[3]},
                    // Format Header
                    {FMTHeaderValue[0], FMTHeaderValue[1], FMTHeaderValue[2], FMTHeaderValue[3]},
                    16,
                    WAVHeaderAudioFormatIEEEFloat,
                    static_cast<short>(channels),
                    sample_rate,
                    static_cast<int>(sample_rate * channels * sizeof(float)),
                    static_cast<short>(channels * sizeof(float)),
                    sizeof(float) * bits_per_byte,
                    // Data Header
                    {DATAHeaderValue[0],
                     DATAHeaderValue[1],
                     DATAHeaderValue[2],
                     DATAHeaderValue[3]},
                    static_cast<int>(samples * sizeof(float))};
                raw_handle.write(reinterpret_cast<char *>(&header), sizeof(WAVHeader));
                raw_handle.write(reinterpret_cast<char *>(samples_ptr), samples * sizeof(float));
                raw_handle.close();
              }
              std::exit(0);
            });
      },
      [](const std::string &domain, int error_code) {
        std::cerr << "Error: " << domain << " " << error_code << std::endl;
        std::exit(error_code);
      });

  while (true) {
    sleep(1);
  }

  return 0;
}
