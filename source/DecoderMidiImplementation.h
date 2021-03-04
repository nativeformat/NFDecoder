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
#include <NFDecoder/Factory.h>

#include <future>
#include <string>

#define TSF_STATIC
#define TSF_IMPLEMENTATION
#include <tsf.h>
#undef TSF_IMPLEMENTATION
#undef TSF_STATIC

#define TML_STATIC
#define TML_IMPLEMENTATION
#include <tml.h>
#undef TML_IMPLEMENTATION
#undef TML_STATIC

namespace nativeformat {
namespace decoder {

class DecoderMidiImplementation : public Decoder,
                                  public std::enable_shared_from_this<DecoderMidiImplementation> {
 public:
  enum class ErrorCode : int { ErrorCodeLoadMIDIFailure, ErrorCodeLoadSoundFontFailure };

  DecoderMidiImplementation(const std::string &path);
  ~DecoderMidiImplementation();

  virtual double sampleRate() final;
  virtual int channels() final;
  virtual long currentFrameIndex() final;
  virtual void seek(long frame_index) final;
  virtual long frames() final;
  virtual void decode(long frames, const DECODE_CALLBACK &decode_callback, bool synchronous) final;
  virtual bool eof() final;
  virtual const std::string &path() final;
  virtual const std::string &name() final;
  virtual void flush() final;
  virtual void load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                    const LOAD_DECODER_CALLBACK &decoder_load_callback) final;

  const tml_message *stream() { return _midi_stream; }
  const tsf *soundbank() { return _soundfont; }

  void load_midi();
  void load_soundfont();

 private:
  std::string midi_prefix = "midi:";
  std::string soundfont_prefix = ":soundfont:";

  std::string _midi_path;
  std::string _soundfont_path;
  // TODO: Replace _midi_stream and _soundfont with custom allocators
  // makes it easier on the destructor
  // TODO: If the midi seeking becomes to slow consider copying the midi data
  // into a vector and freeing the file
  tml_message *_midi_head;
  tml_message *_midi_stream;
  tsf *_soundfont;

  std::atomic<int> _channels;
  std::atomic<double> _samplerate;
  std::atomic<long> _frame_size;
  std::atomic<long> _frame_index;
  std::atomic<long> _frames;
  std::future<void> _load_future;
};

}  // namespace decoder
}  // namespace nativeformat
