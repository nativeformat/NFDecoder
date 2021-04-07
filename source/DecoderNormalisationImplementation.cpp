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
#include "DecoderNormalisationImplementation.h"

#include <cstdlib>
#include <future>

namespace nativeformat {
namespace decoder {

DecoderNormalisationImplementation::DecoderNormalisationImplementation(
    const std::shared_ptr<Decoder> &wrapped_decoder, const double samplerate, const int channels)
    : _wrapped_decoder(wrapped_decoder),
      _factor(0.0),
      _frame_index(0),
      _samplerate(samplerate),
      _channels(channels) {
  for (int i = 0; i < this->channels(); ++i) {
    _resampler_handlers[i] = nullptr;
  }
}

DecoderNormalisationImplementation::~DecoderNormalisationImplementation() {
  for (int i = 0; i < channels(); ++i) {
    void *resample_handler = _resampler_handlers[i];
    if (resample_handler != nullptr) {
      resample_close(resample_handler);
    }
  }
}

const std::string &DecoderNormalisationImplementation::name() {
  static const std::string domain("com.nativeformat.decoder.normalisation");
  return domain;
}

void DecoderNormalisationImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                              const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  {
    std::lock_guard<std::mutex> resampler_lock(_resampler_mutex);
    _factor = sampleRate() / _wrapped_decoder->sampleRate();
    if (_factor != 1.0) {
      for (int i = 0; i < channels(); ++i) {
        _resampler_handlers[i] = resample_open(1, _factor, _factor);
      }
    }
  }
  decoder_load_callback(true);
}

double DecoderNormalisationImplementation::sampleRate() {
  return _samplerate;
}

int DecoderNormalisationImplementation::channels() {
  return _channels;
}

long DecoderNormalisationImplementation::currentFrameIndex() {
  return _frame_index;
}

void DecoderNormalisationImplementation::seek(long frame_index) {
  _frame_index = frame_index;

  std::lock_guard<std::mutex> resampler_lock(_resampler_mutex);
  _wrapped_decoder->seek(frame_index / _factor);

  // Flush the resamplers
  for (int i = 0; i < channels(); ++i) {
    if (_resampler_handlers[i] != nullptr) {
      int out_buffer_size = sampleRate() * channels() * sizeof(float);
      float *out_buffer = (float *)malloc(out_buffer_size);
      int in_buffer_size = 0;
      resample_process(_resampler_handlers[i],
                       _factor,
                       nullptr,
                       0,
                       1,
                       &in_buffer_size,
                       out_buffer,
                       out_buffer_size);
      free(out_buffer);
    }
  }
  _pcm_buffer.clear();
}

long DecoderNormalisationImplementation::frames() {
  const auto wrapped_frames = _wrapped_decoder->frames();
  if (wrapped_frames == UNKNOWN_FRAMES) {
    return UNKNOWN_FRAMES;
  }
  return wrapped_frames * _factor;
}

void DecoderNormalisationImplementation::decode(long frames,
                                                const DECODE_CALLBACK &decode_callback,
                                                bool synchronous) {
  {
    auto samples = frames * channels();
    if (_pcm_buffer.size() >= samples) {
      long frame_index = _frame_index;
      _frame_index = frame_index + frames;
      decode_callback(frame_index, frames, _pcm_buffer.data());
      _pcm_buffer.erase(_pcm_buffer.begin(), _pcm_buffer.begin() + samples);
      return;
    }
  }
  auto strong_this = shared_from_this();
  // Sometimes the normaliser cuts a bit off
  const auto normalised_frames = _factor != 1.0 ? (frames / _factor) * 1.01 : frames;
  _wrapped_decoder->decode(
      normalised_frames,
      [decode_callback, strong_this, frames](long frame_index, long input_frames, float *samples) {
        long sent_frames = 0;
        float *buffered_output = nullptr;
        long current_frame_index = strong_this->currentFrameIndex();
        {
          std::lock_guard<std::mutex> resampler_lock(strong_this->_resampler_mutex);
          const auto channels = strong_this->channels();
          size_t channel_samples_count = input_frames * channels;
          float *channel_samples = (float *)malloc(sizeof(float) * channel_samples_count);
          for (int i = 0; i < channel_samples_count; ++i) {
            channel_samples[i] = 0.0f;
          }

          // Normalise the channels
          const auto decoder_channels = strong_this->_wrapped_decoder->channels();
          if (decoder_channels > channels) {
            // Copy the channels into stereo
            int even_decoder_channels = decoder_channels - (decoder_channels % channels);
            for (long i = 0; i < input_frames; ++i) {
              for (int j = 0; j < even_decoder_channels; ++j) {
                int normalised_channel = j % channels;
                channel_samples[(i * channels) + normalised_channel] +=
                    samples[(i * decoder_channels) + j];
              }
            }
            // Mix the uneven channel into both channels
            if (even_decoder_channels != decoder_channels) {
              for (long i = 0; i < input_frames; ++i) {
                for (int j = 0; j < channels; ++j) {
                  channel_samples[(i * channels) + j] +=
                      samples[(i * decoder_channels) + (decoder_channels - 1)];
                }
              }
            }
            // Lower the volume properly
            float volume_factor = decoder_channels / channels;
            for (long i = 0; i < input_frames; ++i) {
              for (int j = 0; j < channels; ++j) {
                channel_samples[(i * channels) + j] /= volume_factor;
              }
            }
          } else if (decoder_channels < channels) {
            // Copy all the other channels into the redundant channels
            for (int i = 0; i < channels; ++i) {
              for (long j = 0; j < input_frames; ++j) {
                if (i < decoder_channels) {
                  channel_samples[(j * channels) + i] = samples[(j * decoder_channels) + i];
                } else {
                  float sample = 0.0f;
                  for (int k = 0; k < decoder_channels; ++k) {
                    sample += samples[(j * decoder_channels) + k];
                  }
                  sample /= static_cast<float>(decoder_channels);
                  channel_samples[(j * channels) + i] = sample;
                }
              }
            }
          } else {
            memcpy(channel_samples, samples, channel_samples_count * sizeof(float));
          }

          // Resample the channels
          const auto factor = strong_this->_factor;
          if (factor == 1.0) {
            // Short circuit here
            decode_callback(current_frame_index, input_frames, channel_samples);
            free(channel_samples);
            return;
          }

          long new_frames = input_frames * factor + 1;
          auto resampled_output_samples = new_frames * channels;
          size_t resampled_output_size = resampled_output_samples * sizeof(float);
          float *resampled_output = (float *)malloc(resampled_output_size);
          if (new_frames - 1 == input_frames) {
            memcpy(resampled_output, channel_samples, resampled_output_size);
            new_frames = input_frames;
          } else {
            bool eof = strong_this->_wrapped_decoder->eof();
            size_t old_channel_samples_size = input_frames * sizeof(float);
            size_t resampled_samples_size = new_frames * sizeof(float);
            float *old_channel_samples = (float *)malloc(old_channel_samples_size);
            float *resampled_samples = (float *)malloc(resampled_samples_size);
            for (int i = 0; i < channels; ++i) {
              void *resample_handler = strong_this->_resampler_handlers[i];
              if (resample_handler != nullptr) {
                for (long j = 0; j < input_frames; ++j) {
                  old_channel_samples[j] = channel_samples[(j * channels) + i];
                }
                int buffer_used = 0;
                int sample_count = resample_process(resample_handler,
                                                    factor,
                                                    old_channel_samples,
                                                    input_frames,
                                                    eof,
                                                    &buffer_used,
                                                    resampled_samples,
                                                    new_frames);
                new_frames = std::min(static_cast<long>(sample_count), new_frames);
                for (long j = 0; j < new_frames; ++j) {
                  float sample = resampled_samples[j];
                  resampled_output[(j * channels) + i] = sample;
                }
              } else {
                // This shouldn't happen... but if it does do a regular copy
                long max_frames = std::min(new_frames, input_frames);
                for (long j = 0; j < max_frames; ++j) {
                  resampled_output[(j * channels) + i] = channel_samples[(j * channels) + i];
                }
              }
            }
            free(old_channel_samples);
            free(resampled_samples);
          }

          auto buffered_output_samples = frames * channels;
          buffered_output = (float *)malloc(buffered_output_samples * sizeof(float));
          auto cached_buffer_samples = std::min((long)strong_this->_pcm_buffer.size(), new_frames);
          memcpy(buffered_output,
                 strong_this->_pcm_buffer.data(),
                 cached_buffer_samples * sizeof(float));
          strong_this->_pcm_buffer.erase(strong_this->_pcm_buffer.begin(),
                                         strong_this->_pcm_buffer.begin() + cached_buffer_samples);
          auto resampled_output_used_samples =
              std::min(buffered_output_samples - cached_buffer_samples, resampled_output_samples);
          memcpy(&buffered_output[cached_buffer_samples],
                 resampled_output,
                 resampled_output_used_samples * sizeof(float));
          auto resampled_output_left_samples =
              resampled_output_samples - resampled_output_used_samples;
          float *leftover_output = resampled_output + resampled_output_used_samples;
          strong_this->_pcm_buffer.insert(strong_this->_pcm_buffer.end(),
                                          leftover_output,
                                          leftover_output + resampled_output_left_samples);
          free(resampled_output);
          free(channel_samples);

          sent_frames = (resampled_output_used_samples + cached_buffer_samples) / channels;
          if (sent_frames == 1 && frames != 1) {
            sent_frames = 0;
            strong_this->_pcm_buffer.clear();
          } else {
            strong_this->_frame_index = current_frame_index + sent_frames;
          }
        }

        decode_callback(current_frame_index, sent_frames, buffered_output);
        free(buffered_output);
      },
      synchronous);
}

bool DecoderNormalisationImplementation::eof() {
  std::lock_guard<std::mutex> resampler_lock(_resampler_mutex);
  return _wrapped_decoder->eof() && _pcm_buffer.empty();
}

const std::string &DecoderNormalisationImplementation::path() {
  return _wrapped_decoder->path();
}

void DecoderNormalisationImplementation::flush() {
  std::lock_guard<std::mutex> resampler_lock(_resampler_mutex);
  _pcm_buffer.clear();
  _wrapped_decoder->flush();
  for (int i = 0; i < channels(); ++i) {
    if (_resampler_handlers[i] != nullptr) {
      int out_buffer_size = sampleRate() * channels() * sizeof(float);
      float *out_buffer = (float *)malloc(out_buffer_size);
      int in_buffer_size = 0;
      resample_process(_resampler_handlers[i],
                       _factor,
                       nullptr,
                       0,
                       1,
                       &in_buffer_size,
                       out_buffer,
                       out_buffer_size);
      free(out_buffer);
    }
  }
}

}  // namespace decoder
}  // namespace nativeformat
