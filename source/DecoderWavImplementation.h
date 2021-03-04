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

#include <NFDecoder/Decoder.h>

#include <atomic>
#include <future>
#include <mutex>

#include <NFDecoder/DataProvider.h>
#include <NFDecoder/Factory.h>

namespace nativeformat {
namespace decoder {

typedef std::function<void(bool success)> LOAD_DECODER_CALLBACK;

class DecoderWavImplementation : public Decoder,
                                 public std::enable_shared_from_this<DecoderWavImplementation> {
 public:
  typedef enum : int {
    ErrorCodeNotEnoughDataForHeader,
    ErrorCodeCouldNotDecodeHeader,
    ErrorCodeNotRiff,
    ErrorCodeNotWav,
    ErrorCodeChunkError
  } ErrorCode;

  DecoderWavImplementation(std::shared_ptr<DataProvider> &data_provider);
  virtual ~DecoderWavImplementation();

  // Decoder
  virtual double sampleRate();
  virtual int channels();
  virtual long currentFrameIndex();
  virtual void seek(long frame_index);
  virtual long frames();
  virtual void decode(long frames, const DECODE_CALLBACK &decode_callback, bool synchronous);
  virtual bool eof();
  virtual const std::string &path();
  virtual const std::string &name();
  virtual void flush();
  virtual void load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                    const LOAD_DECODER_CALLBACK &decoder_load_callback);

 private:
  typedef enum : short {
    WAVHeaderAudioFormatNone = 0,
    WAVHeaderAudioFormatPCM = 1,
    WAVHeaderAudioFormatIEEEFloat = 3
  } WAVHeaderAudioFormat;

#pragma pack(push, 1)
  typedef struct WAVHeader {
    // RIFF
    char riff_header_name[4];
    uint32_t file_size;
    char wave_header_name[4];
  } WAVHeader;

  typedef struct FMTHeader {
    uint32_t chunk_size;
    WAVHeaderAudioFormat audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t sample_alignment;
    uint16_t bit_depth;
  } FMTHeader;

  typedef struct DATAHeader {
    uint32_t data_bytes;
  } DATAHeader;
#pragma pack(pop)

  bool readChunk();
  bool knownType();
  static size_t wavSampleSize(const FMTHeader &header);

  std::shared_ptr<DataProvider> _data_provider;
  const ERROR_DECODER_CALLBACK _decoder_error_callback;

  std::mutex _wav_mutex;
  std::atomic<int> _channels;
  std::atomic<double> _samplerate;
  std::atomic<long> _frames;
  std::atomic<long> _frame_size;
  std::atomic<long> _frame_index;
  std::future<void> _load_future;
  char _chunk_type[4];
  WAVHeader _header;
  FMTHeader _fmt;
  size_t _data_offset;
  uint32_t _data_bytes;
};

template <typename sample_t>
struct WavReader {
  std::vector<sample_t> in_samples;
  std::vector<float> out_samples;
  DataProvider *dp;
  static constexpr size_t sample_size = sizeof(sample_t);

  WavReader(DataProvider *data_provider, size_t frames, size_t channels)
      : in_samples(frames * channels), dp(data_provider) {}
  ~WavReader() {}

  size_t transferSamples(size_t frames, size_t channels) {
    size_t bytes_read = dp->read((char *)in_samples.data(), sample_size * channels, frames);
    size_t frames_read = bytes_read / (sample_size * channels);
    out_samples.resize(frames_read * channels);
    static constexpr float s_min = static_cast<float>(std::numeric_limits<sample_t>::min());
    static constexpr float s_max = static_cast<float>(std::numeric_limits<sample_t>::max());
    static constexpr sample_t dc_offset = s_min ? 0 : s_max / 2;
    for (long i = 0; i < frames_read * channels; ++i) {
      float sample = static_cast<float>(in_samples[i] - dc_offset) /
                     static_cast<float>(std::numeric_limits<sample_t>::max());
      out_samples[i] = sample;
    }
    return frames_read;
  }
};

}  // namespace decoder
}  // namespace nativeformat
