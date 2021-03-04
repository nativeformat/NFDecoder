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
#include "DecoderMidiImplementation.h"
#include <iostream>
#include <vector>

namespace nativeformat {
namespace decoder {

DecoderMidiImplementation::DecoderMidiImplementation(const std::string &path)
    : _midi_path(path.begin() + path.find(midi_prefix) + midi_prefix.size(),
                 path.begin() + path.find(soundfont_prefix)),
      _soundfont_path(path.begin() + path.find(soundfont_prefix) + soundfont_prefix.size(),
                      path.end()),
      _channels(2),
      _samplerate(44100.0),
      _frame_size(0),
      _frame_index(0),
      _frames(0) {}

DecoderMidiImplementation::~DecoderMidiImplementation() {
  tml_free(_midi_head);
  tsf_close(_soundfont);
}

const std::string &DecoderMidiImplementation::name() {
  static const std::string domain("com.nativeformat.decoder.midi");
  return domain;
}

const std::string &DecoderMidiImplementation::path() {
  return _midi_path;
}

void DecoderMidiImplementation::load_midi() {
  _midi_head = tml_load_filename(_midi_path.c_str());
  _midi_stream = _midi_head;
}

void DecoderMidiImplementation::load_soundfont() {
  _soundfont = tsf_load_filename(_soundfont_path.c_str());
  tsf_set_output(_soundfont, TSF_STEREO_INTERLEAVED, sampleRate());
}

void DecoderMidiImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                     const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  std::shared_ptr<DecoderMidiImplementation> strong_this = shared_from_this();
  _load_future = std::async(
      std::launch::async, [strong_this, decoder_error_callback, decoder_load_callback]() {
        strong_this->load_midi();
        if (strong_this->stream() == nullptr) {
          decoder_error_callback(
              strong_this->name(),
              (int)DecoderMidiImplementation::ErrorCode::ErrorCodeLoadMIDIFailure);
          decoder_load_callback(false);
        }

        // figure out how many frames of data we have
        const tml_message *tmp = strong_this->stream();
        while (tmp->next != nullptr) {
          tmp = tmp->next;
        }
        strong_this->_frames = (1. / 1.e3) * tmp->time * strong_this->sampleRate();

        strong_this->load_soundfont();
        if (strong_this->soundbank() == nullptr) {
          decoder_error_callback(
              strong_this->name(),
              (int)DecoderMidiImplementation::ErrorCode::ErrorCodeLoadSoundFontFailure);
          decoder_load_callback(false);
        }
        strong_this->seek(0);
        decoder_load_callback(true);
      });
}

double DecoderMidiImplementation::sampleRate() {
  return _samplerate;
}

int DecoderMidiImplementation::channels() {
  return _channels;
}

long DecoderMidiImplementation::currentFrameIndex() {
  return _frame_index;
}

void DecoderMidiImplementation::seek(long frame_index) {
  if (frame_index == _frame_index) return;

  // For now we'll do a shlemiel the painter until caching all the
  // messages into a vector seems reasonable
  long desired_time = frame_index / sampleRate();
  if (frame_index < _frame_index || frame_index < desired_time) {
    _frame_index = 0;
    _midi_stream = _midi_head;
  }

  // because the frame boundary may be in between two midi messages we
  // search for when the next message cross the time boundary
  tml_message *curr = _midi_stream;
  tml_message *next = _midi_stream->next;

  while (next && next->time < desired_time) {
    ++_frame_index;
    next = next->next;
    curr = curr->next;
  }
  _midi_stream = curr;

  _frame_index = frame_index;

  if (!next) {
    _midi_stream = nullptr;
  }
}

long DecoderMidiImplementation::frames() {
  return _frames;
}

void DecoderMidiImplementation::decode(long frames,
                                       const DECODE_CALLBACK &decode_callback,
                                       bool synchronous) {
  long frame_index = _frame_index;
  if (_midi_stream == nullptr) {
    decode_callback(frame_index, 0, nullptr);
  }

  std::shared_ptr<DecoderMidiImplementation> strong_this = shared_from_this();
  auto run_thread = [strong_this, decode_callback, frames, frame_index] {
    if (frames == 0) {
      decode_callback(frame_index, 0, nullptr);
      return;
    }
    int channels = strong_this->channels();
    if (channels == 0) {
      decode_callback(frame_index, 0, nullptr);
      return;
    }

    double samplerate = strong_this->sampleRate();
    const tml_message *midi_pkt = strong_this->stream();
    auto *sounds = const_cast<tsf *>(strong_this->soundbank());
    const double time_incr = 1000.0 / samplerate;
    double time_ms = frame_index * time_incr;
    int frames_left = frames;

    std::vector<float> output(frames * channels);

    int frame_block = 64;  // Recommended value from tsf.h. Lower means more
                           // accurate but more CPU required.
    for (size_t output_index = 0; frames_left;
         frames_left -= frame_block, output_index += frame_block * channels) {
      // We progress the MIDI playback and then process
      // TSF_RENDER_EFFECTSAMPLEBLOCK samples at once
      if (frame_block > frames_left) frame_block = frames_left;

      // Loop through all MIDI messages which need to be played up until the
      // current playback time
      // 1000 for milliseconds
      for (time_ms += frame_block * time_incr; midi_pkt && time_ms >= midi_pkt->time;
           midi_pkt = midi_pkt->next) {
        switch (midi_pkt->type) {
          case TML_NOTE_OFF:  // stop a note
            tsf_channel_note_off(sounds, midi_pkt->channel, midi_pkt->key);
            break;
          case TML_NOTE_ON:  // play a note
            tsf_channel_note_on(
                sounds, midi_pkt->channel, midi_pkt->key, midi_pkt->velocity / 127.0f);
            break;
          case TML_KEY_PRESSURE:
            // TODO: Implement
            break;
          case TML_CONTROL_CHANGE:  // MIDI controller messages
            tsf_channel_midi_control(
                sounds, midi_pkt->channel, midi_pkt->control, midi_pkt->control_value);
            break;
          case TML_PROGRAM_CHANGE:  // channel program (preset) change (special
                                    // handling for 10th MIDI channel with drums)
            tsf_channel_set_presetnumber(
                sounds, midi_pkt->channel, midi_pkt->program, (midi_pkt->channel == 9));
            break;
          case TML_CHANNEL_PRESSURE:
          // TODO: Implement
          case TML_PITCH_BEND:  // pitch wheel modification
            tsf_channel_set_pitchwheel(sounds, midi_pkt->channel, midi_pkt->pitch_bend);
            break;
        }
      }

      // Render the block of audio samples in float format
      tsf_render_float(sounds, output.data() + output_index, frame_block, 0);
    }
    decode_callback(frame_index, frames - frames_left, output.data());
  };
  if (synchronous) {
    run_thread();
  } else {
    std::thread(run_thread).detach();
  }
}

bool DecoderMidiImplementation::eof() {
  return _midi_stream == nullptr;
}

void DecoderMidiImplementation::flush() {}

}  // namespace decoder
}  // namespace nativeformat
