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
#include "DecoderDashToHLSTransmuxerImplementation.h"

#if INCLUDE_UDT

#include <thread>

#include "DecoderAVCodecImplementation.h"

namespace nativeformat {
namespace decoder {

std::atomic<long> DecoderDashToHLSTransmuxerImplementation::_next{0};

DecoderDashToHLSTransmuxerImplementation::DecoderDashToHLSTransmuxerImplementation(
    const std::shared_ptr<DataProvider> &data_provider,
    const std::shared_ptr<DataProviderFactory> &data_provider_factory,
    const std::string &path,
    const std::shared_ptr<Factory> &factory,
    const std::shared_ptr<Manifest> &manifest,
    const std::shared_ptr<Decrypter> &decrypter)
    : _id(_next++),
      _data_provider(data_provider),
      _data_provider_factory(data_provider_factory),
      _factory(factory),
      _manifest(manifest),
      _decrypter(decrypter),
      _data_provider_memory(std::make_shared<DataProviderMemoryImplementation>(path)),
      _session(nullptr),
      _index(nullptr),
      _frame_index(0),
      _start_junk_frames(1024) {
  DashToHls_CreateSession(&_session);
  DashToHls_SetCenc_PsshHandler(
      _session, nullptr, [](void *, const uint8_t *, size_t) -> DashToHlsStatus {
        return kDashToHlsStatus_OK;
      });
  if (decrypter) {
    DashToHls_SetCenc_DecryptSample(
        _session,
        decrypter.get(),
        [](DashToHlsContext context,
           const uint8_t *encrypted,
           uint8_t *clear,
           size_t length,
           uint8_t *iv,
           size_t iv_length,
           const uint8_t *key_id,
           // key_id is always 16 bytes.
           struct SampleEntry *,
           size_t sampleEntrySize) {
          Decrypter *decrypter = (Decrypter *)context;
          std::vector<unsigned char> input(encrypted, encrypted + length);
          std::vector<unsigned char> output(length, 0);
          auto status = decrypter->decrypt(input, output, key_id, 16, iv, iv_length);
          if (status != DECRYPTER_SUCCESS) {
            return kDashToHlsStatus_BadConfiguration;
          }
          memcpy(clear, output.data(), length);
          return kDashToHlsStatus_OK;
        },
        false);
  }
}

DecoderDashToHLSTransmuxerImplementation::~DecoderDashToHLSTransmuxerImplementation() {
  DashToHls_ReleaseSession(_session);
}

const std::string &DecoderDashToHLSTransmuxerImplementation::name() {
  static const std::string domain("com.nativeformat.decoder.dash2hlstransmuxer");
  return domain;
}

void DecoderDashToHLSTransmuxerImplementation::loadSegment(
    int segment_index,
    const ERROR_DECODER_CALLBACK &decoder_error_callback,
    const EXHAUST_CALLBACK &exhaust_callback) {
  auto status = writeSegment(segment_index);
  if (status != kDashToHlsStatus_OK) {
    decoder_error_callback(name(), status);
    return;
  }

  exhaustDecoder(segment_index, exhaust_callback);
}

DashToHlsStatus DecoderDashToHLSTransmuxerImplementation::writeSegment(int segment_index) {
  auto segment = _index->segments[segment_index];
  _data_provider->seek(segment.location, SEEK_SET);
  size_t segment_length = segment.length;
  unsigned char *segment_data = (unsigned char *)malloc(segment_length);
  size_t segment_true_length =
      _data_provider->read(segment_data, sizeof(unsigned char), segment_length);
  const uint8_t *hls_segment = nullptr;
  size_t hls_length = 0;
  DashToHlsStatus status = DashToHls_ConvertDashSegment(
      _session, segment_index, segment_data, segment_true_length, &hls_segment, &hls_length);
  free(segment_data);
  if (status != kDashToHlsStatus_OK) {
    return status;
  }
  _data_provider_memory->write((void *)hls_segment, sizeof(unsigned char), hls_length);
  return status;
}

void DecoderDashToHLSTransmuxerImplementation::exhaustDecoder(
    int segment_index, const EXHAUST_CALLBACK &exhaust_callback) {
  auto segment = _index->segments[segment_index];
  double time =
      static_cast<double>(segment.duration) * (1.0 / static_cast<double>(segment.timescale));
  long frames = time * sampleRate();
  auto strong_this = shared_from_this();
  _decoder->decode(frames,
                   [strong_this, exhaust_callback, frames, segment_index](
                       long frame_index, long frame_count, float *samples) {
                     strong_this->_samples.insert(
                         strong_this->_samples.end(),
                         samples,
                         samples + (frame_count * strong_this->channels()));
                     exhaust_callback();
                   });
}

double DecoderDashToHLSTransmuxerImplementation::sampleRate() {
  return _decoder->sampleRate();
}

int DecoderDashToHLSTransmuxerImplementation::channels() {
  return _decoder->channels();
}

long DecoderDashToHLSTransmuxerImplementation::currentFrameIndex() {
  return _frame_index;
}

void DecoderDashToHLSTransmuxerImplementation::seek(long frame_index) {
  std::lock_guard<std::mutex> lock(_decoding_mutex);
  long safe_frame_index = std::min(frame_index, frames() - 1);
  long previous_frame_index = _frame_index;
  _frame_index = safe_frame_index;

  // Are we going forward in time?
  if (previous_frame_index < safe_frame_index) {
    long frame_diff = safe_frame_index - previous_frame_index;
    // Do we have enough samples in our buffer to support this new frame index?
    long sample_diff = frame_diff * channels();
    if (sample_diff < _samples.size()) {
      _samples.erase(_samples.begin(), _samples.begin() + sample_diff);
      return;
    }
  } else if (previous_frame_index == safe_frame_index) {
    return;
  }

  // Erase all our current samples
  _samples.clear();
}

long DecoderDashToHLSTransmuxerImplementation::frames() {
  double time = 0.0;
  for (uint32_t i = 0; i < _index->index_count; ++i) {
    auto segment = _index->segments[i];
    time += static_cast<double>(segment.duration) * (1.0 / static_cast<double>(segment.timescale));
  }
  return (time * sampleRate()) - _start_junk_frames;
}

void DecoderDashToHLSTransmuxerImplementation::decode(long frames,
                                                      const DECODE_CALLBACK &decode_callback,
                                                      bool synchronous) {
  auto strong_this = shared_from_this();
  auto run_thread = [strong_this, decode_callback, frames] {
    std::lock_guard<std::mutex> lock(strong_this->_decoding_mutex);
    long frame_index = strong_this->currentFrameIndex();
    long possible_frames = std::min(frames, strong_this->frames() - frame_index);
    auto current_frame_index =
        static_cast<long>(frame_index + (strong_this->_samples.size() / strong_this->channels()));
    while (possible_frames > (strong_this->_samples.size() / strong_this->channels())) {
      // Find segment to load
      uint32_t i = 0;
      auto time_frame_index = 0l;
      auto start_time_frame_index = 0l;
      for (; i < strong_this->_index->index_count; ++i) {
        auto segment = strong_this->_index->segments[i];
        auto time =
            static_cast<double>(segment.duration) * (1.0 / static_cast<double>(segment.timescale));
        time_frame_index += time * strong_this->sampleRate();
        if (i == 0) {
          time_frame_index -= strong_this->_start_junk_frames;
        }
        if (time_frame_index > current_frame_index) {
          break;
        }
        start_time_frame_index = time_frame_index;
      }

      // Sometimes we get segments that have subseconds of content (making the
      // floor check fail)
      if (i == strong_this->_index->index_count) {
        i--;
      }

      // Load next segment and wait
      auto current_frames = strong_this->_samples.size() / strong_this->channels();
      std::condition_variable conditional_variable;
      std::mutex mutex;
      bool error = false, loaded = false;
      strong_this->loadSegment(
          i,
          [&conditional_variable, &mutex, &error, &loaded](const std::string &domain,
                                                           int error_code) {
            std::lock_guard<std::mutex> lock(mutex);
            loaded = true;
            error = true;
            conditional_variable.notify_one();
          },
          [&conditional_variable, &mutex, &loaded]() {
            std::lock_guard<std::mutex> lock(mutex);
            loaded = true;
            conditional_variable.notify_one();
          });
      std::unique_lock<std::mutex> lock(mutex);
      while (!loaded) {
        conditional_variable.wait(lock);
      }

      // Remove start junk samples
      if (current_frame_index < strong_this->_start_junk_frames) {
        auto frames_to_remove =
            std::min(strong_this->_start_junk_frames - current_frame_index,
                     static_cast<long>(strong_this->_samples.size() / strong_this->channels()));
        auto samples_to_remove = frames_to_remove * strong_this->channels();
        strong_this->_samples.erase(strong_this->_samples.begin(),
                                    strong_this->_samples.begin() + samples_to_remove);
      }

      // Clip to the nearest segment
      if (current_frames == 0 && current_frame_index > start_time_frame_index) {
        auto frames_to_skip = static_cast<long>(current_frame_index - start_time_frame_index);
        auto samples_to_skip = frames_to_skip * strong_this->channels();
        strong_this->_samples.erase(strong_this->_samples.begin(),
                                    strong_this->_samples.begin() + samples_to_skip);
      }

      if (error) {
        break;
      }
      ++i;
      current_frame_index = frame_index + (strong_this->_samples.size() / strong_this->channels());
    }

    // Fill Sample buffer
    long output_frames = std::min(
        possible_frames, static_cast<long>(strong_this->_samples.size() / strong_this->channels()));
    strong_this->_frame_index = frame_index + output_frames;
    decode_callback(frame_index, output_frames, &strong_this->_samples[0]);
    strong_this->_samples.erase(
        strong_this->_samples.begin(),
        strong_this->_samples.begin() + (output_frames * strong_this->channels()));
  };
  if (synchronous) {
    run_thread();
  } else {
    std::thread(run_thread).detach();
  }
}

bool DecoderDashToHLSTransmuxerImplementation::eof() {
  return frames() <= currentFrameIndex();
}

const std::string &DecoderDashToHLSTransmuxerImplementation::path() {
  return _data_provider->path();
}

void DecoderDashToHLSTransmuxerImplementation::load(
    const ERROR_DECODER_CALLBACK &decoder_error_callback,
    const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  auto strong_this = shared_from_this();
  std::thread([strong_this, decoder_error_callback, decoder_load_callback] {
    // Load the seek table
    std::vector<int> index_range = {0, 500 * 1024};
    if (strong_this->_manifest) {
      nlohmann::json seek_table = strong_this->_manifest->json()["seekTable"];
      index_range = seek_table["index_range"].get<std::vector<int>>();
    }
    // TODO: Figure out why the index range is not enough "sometimes"
    size_t data_size = index_range.back() * 2;
    unsigned char *data = (unsigned char *)malloc(data_size);
    size_t data_read = strong_this->_data_provider->read(data, sizeof(unsigned char), data_size);
    DashToHlsStatus status =
        DashToHls_ParseDash(strong_this->_session, data, data_read, &strong_this->_index);
    free(data);
    if (status != kDashToHlsStatus_OK && status != kDashToHlsStatus_ClearContent) {
      decoder_error_callback(strong_this->name(), status);
      decoder_load_callback(false);
      return;
    }

    status = strong_this->writeSegment(0);
    if (status != kDashToHlsStatus_OK) {
      decoder_error_callback(strong_this->name(), status);
      return;
    }

    // Create a MPEG2TS Decoder
    auto creator_index = strong_this->_data_provider_factory->addDataProviderCreator(
        [strong_this](const std::string &path) -> std::shared_ptr<DataProvider> {
          if (path == strong_this->fake_path()) {
            return strong_this->_data_provider_memory;
          }
          return nullptr;
        });
    strong_this->_factory->createDecoder(
        strong_this->fake_path(),
        NF_DECODER_MIME_TYPE_AUDIO_MP2TS,
        [strong_this, creator_index, decoder_load_callback](
            const std::shared_ptr<Decoder> &decoder) {
          strong_this->_data_provider_factory->removeDataProviderCreator(creator_index);
          strong_this->_decoder = decoder;
          bool is_avcodec_decoder = false;
#if INCLUDE_LGPL
          is_avcodec_decoder = decoder->name() == DECODER_AVCODEC_NAME;
#endif
          if (!is_avcodec_decoder) {
            strong_this->exhaustDecoder(0, [strong_this, decoder_load_callback]() {
              strong_this->seek(0);
              strong_this->_samples.clear();
              strong_this->_data_provider_memory->flush();
              strong_this->_decoder->flush();
              decoder_load_callback(true);
            });
          } else {
            strong_this->seek(0);
            strong_this->_data_provider_memory->flush();
            strong_this->_decoder->flush();
            decoder_load_callback(true);
          }
        },
        decoder_error_callback);
  }).detach();
}

void DecoderDashToHLSTransmuxerImplementation::flush() {
  _samples.clear();
  _data_provider_memory->flush();
  return _decoder->flush();
}

std::string DecoderDashToHLSTransmuxerImplementation::fake_path() {
  static const std::string fake_path_prefix("MyNQXMWg");
  return fake_path_prefix + std::to_string(_id);
}

}  // namespace decoder
}  // namespace nativeformat

namespace dash2hls {

void DashToHlsDefaultDiagnosticCallback(const char *message) {
  printf("%s\n", message);
}

}  // namespace dash2hls

#endif
