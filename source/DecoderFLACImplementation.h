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

#include <NFDecoder/Decoder.h>

#include <atomic>
#include <future>
#include <mutex>
#include <vector>

#include <NFDecoder/DataProvider.h>
#include <NFDecoder/Factory.h>

#include <FLAC/all.h>

namespace nativeformat {
namespace decoder {

typedef std::function<void(bool success)> LOAD_DECODER_CALLBACK;

class DecoderFLACImplementation : public Decoder,
                                  public std::enable_shared_from_this<DecoderFLACImplementation> {
 public:
  typedef enum : int { ErrorCodeNotEnoughData, ErrorCodeCouldNotDecode } ErrorCode;

  DecoderFLACImplementation(std::shared_ptr<DataProvider> &data_provider);
  virtual ~DecoderFLACImplementation();

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
  static FLAC__StreamDecoderReadStatus flac_read(const FLAC__StreamDecoder *decoder,
                                                 FLAC__byte buffer[],
                                                 size_t *bytes,
                                                 void *client_data);
  static FLAC__StreamDecoderSeekStatus flac_seek(const FLAC__StreamDecoder *decoder,
                                                 FLAC__uint64 absolute_byte_offset,
                                                 void *client_data);
  static FLAC__StreamDecoderTellStatus flac_tell(const FLAC__StreamDecoder *decoder,
                                                 FLAC__uint64 *absolute_byte_offset,
                                                 void *client_data);
  static FLAC__StreamDecoderLengthStatus flac_length(const FLAC__StreamDecoder *decoder,
                                                     FLAC__uint64 *stream_length,
                                                     void *client_data);
  static FLAC__bool flac_eof(const FLAC__StreamDecoder *decoder, void *client_data);
  static FLAC__StreamDecoderWriteStatus flac_write(const FLAC__StreamDecoder *decoder,
                                                   const FLAC__Frame *frame,
                                                   const FLAC__int32 *const buffer[],
                                                   void *client_data);
  static void flac_metadata(const FLAC__StreamDecoder *decoder,
                            const FLAC__StreamMetadata *metadata,
                            void *client_data);
  static void flac_error(const FLAC__StreamDecoder *decoder,
                         FLAC__StreamDecoderErrorStatus status,
                         void *client_data);

  std::shared_ptr<DataProvider> _data_provider;

  FLAC__StreamDecoder *_flac_decoder;
  std::mutex _flac_decoder_mutex;
  std::future<void> _load_future;
  std::atomic<int> _channels;
  std::atomic<double> _samplerate;
  std::atomic<long> _frame_index;
  std::atomic<long> _frames;
  std::vector<float> _samples;
  std::mutex _samples_mutex;
};

}  // namespace decoder
}  // namespace nativeformat
