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

#if __APPLE__

#include <atomic>
#include <future>
#include <mutex>
#include <vector>

#include <NFDecoder/DataProvider.h>
#include <NFDecoder/Factory.h>

#import <AudioToolbox/AudioToolbox.h>

namespace nativeformat {
namespace decoder {

class DecoderAudioConverterImplementation
    : public Decoder,
      public std::enable_shared_from_this<DecoderAudioConverterImplementation> {
 public:
  typedef enum : int { ErrorCodeNotEnoughDataForHeader, ErrorCodeCouldNotDecodeHeader } ErrorCode;

  DecoderAudioConverterImplementation(std::shared_ptr<DataProvider> &data_provider);
  virtual ~DecoderAudioConverterImplementation();

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
  static void listenerProc(void *inClientData,
                           AudioFileStreamID inAudioFileStream,
                           AudioFileStreamPropertyID inPropertyID,
                           AudioFileStreamPropertyFlags *ioFlags);
  static void sampleProc(void *inClientData,
                         UInt32 inNumberBytes,
                         UInt32 inNumberPackets,
                         const void *inInputData,
                         AudioStreamPacketDescription *inPacketDescriptions);
  static OSStatus inputDataProc(AudioConverterRef inAudioConverter,
                                UInt32 *ioNumberDataPackets,
                                AudioBufferList *ioData,
                                AudioStreamPacketDescription **outDataPacketDescription,
                                void *inUserData);

  std::shared_ptr<DataProvider> _data_provider;

  std::future<void> _load_future;
  AudioFileStreamID _audio_file_stream;
  std::atomic<bool> _audio_converter_setup_complete;
  AudioConverterRef _audio_converter;
  std::atomic<int> _channels;
  std::atomic<double> _samplerate;
  AudioStreamBasicDescription _output_format;
  AudioBuffer _input_buffer;
  UInt32 _input_packets;
  AudioStreamPacketDescription *_packet_descriptions;
  std::atomic<long> _frame_index;
  std::mutex _audiotoolbox_mutex;
  std::atomic<long> _frames;
  long _frame_offset;
  std::vector<float> _pcm_buffer;
  long _start_junk_frames;
};

}  // namespace decoder
}  // namespace nativeformat

#endif
