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

#if INCLUDE_LGPL

#include <atomic>
#include <future>
#include <mutex>
#include <vector>

#include <NFDecoder/DataProvider.h>
#include <NFDecoder/Decrypter.h>
#include <NFDecoder/Factory.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavresample/avresample.h>
#include <libavutil/opt.h>
}

namespace nativeformat {
namespace decoder {

extern const std::string DECODER_AVCODEC_NAME;

class DecoderAVCodecImplementation
    : public Decoder,
      public std::enable_shared_from_this<DecoderAVCodecImplementation> {
 public:
  typedef enum : int { ErrorCodeCouldNotDecodeHeader } ErrorCode;

  DecoderAVCodecImplementation(const std::shared_ptr<DataProvider> &data_provider,
                               const std::shared_ptr<Decrypter> &decrypter);
  virtual ~DecoderAVCodecImplementation();

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
#pragma pack(push, 1)
  typedef struct SIDX_FRAME {
    uint32_t junk;
    uint32_t subsegment_duration;
    uint32_t referenced_size;
  } SIDX_FRAME;
#pragma pack(pop)
  struct MOOFS {
    MOOFS() : offset(0) {}
    int offset;
    std::vector<SIDX_FRAME> _sidx_frames;
  };
  virtual void runDecodeThread(long frames, const DECODE_CALLBACK &decode_callback);
  static int avio_read(void *opaque, uint8_t *buf, int buf_size);
  static int64_t avio_seek(void *opaque, int64_t offset, int whence);

  int moofIndex(size_t byte_offset);

  std::shared_ptr<DataProvider> _data_provider;
  const std::shared_ptr<Decrypter> _decrypter;

  std::future<void> _load_future;
  std::atomic<long> _frame_index;
  std::atomic<long> _frames;
  unsigned char *_io_context_buffer;
  AVIOContext *_io_context;
  AVFormatContext *_format_context;
  AVAudioResampleContext *_resample_context;
  AVCodecContext *_codec_context;
  std::mutex _av_mutex;
  std::vector<float> _pcm_buffer;
  unsigned char *_key_id;
  size_t _key_id_length;
  AVStream *_stream;
  int _start_junk_frames;
  long _frames_per_entry_index;
  bool _found_sidx;
  std::map<int, uint64_t> _ivs;
  MOOFS _moofs;
  long _packets_per_moof;
};

}  // namespace decoder
}  // namespace nativeformat

#endif
