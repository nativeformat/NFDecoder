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
#include "DecoderAVCodecImplementation.h"

#if INCLUDE_LGPL

#include <cstdlib>
#include <future>
#include <iostream>

namespace nativeformat {
namespace decoder {

namespace {

static const int INVALID_MOOF_INDEX = -1;

static void xorSwap(uint8_t *x, uint8_t *y) {
  if (x != y) {
    *x ^= *y;
    *y ^= *x;
    *x ^= *y;
  }
}

static void swap_endians(uint8_t *memory, size_t memory_size) {
  for (auto i = 0; i < memory_size / 2; ++i) {
    xorSwap(&memory[i], &memory[memory_size - i - 1]);
  }
}

}  // namespace

const std::string DECODER_AVCODEC_NAME("com.nativeformat.decoder.avcodec");

DecoderAVCodecImplementation::DecoderAVCodecImplementation(
    const std::shared_ptr<DataProvider> &data_provider, const std::shared_ptr<Decrypter> &decrypter)
    : _data_provider(data_provider),
      _decrypter(decrypter),
      _frame_index(0),
      _io_context_buffer(nullptr),
      _io_context(nullptr),
      _format_context(nullptr),
      _resample_context(nullptr),
      _codec_context(nullptr),
      _key_id(nullptr),
      _key_id_length(0),
      _stream(nullptr),
      _start_junk_frames(0),
      _frames_per_entry_index(0),
      _found_sidx(false),
      _packets_per_moof(0) {}

DecoderAVCodecImplementation::~DecoderAVCodecImplementation() {
  if (_resample_context != nullptr) {
    avresample_close(_resample_context);
    avresample_free(&_resample_context);
  }

  if (_format_context != nullptr) {
    avformat_free_context(_format_context);
  }

  if (_io_context != nullptr) {
    if (_io_context_buffer != nullptr) {
      // Workaround for double free:
      // https://ffmpeg.org/pipermail/libav-user/2013-April/004162.html
      if (_io_context->buffer == _io_context_buffer) {
        av_freep(&_io_context_buffer);
      }
    }
    avio_closep(&_io_context);
  }

  if (_key_id != nullptr) {
    free(_key_id);
    _key_id_length = 0;
  }
}

const std::string &DecoderAVCodecImplementation::name() {
  return DECODER_AVCODEC_NAME;
}

void DecoderAVCodecImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                        const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  auto strong_this = shared_from_this();
  _load_future = std::async(
      std::launch::async, [strong_this, decoder_error_callback, decoder_load_callback]() {
        static const size_t avio_context_buffer_size = 8192;
        static std::once_flag avcodec_register_all_once;
        static std::once_flag av_register_all_once;

        std::call_once(avcodec_register_all_once, &avcodec_register_all);
        std::call_once(av_register_all_once, &av_register_all);
#if NDEBUG
        static std::once_flag av_log_once;
        std::call_once(av_log_once, []() {
          av_log_set_callback([](void *data, int log_level, const char *format, va_list args) {
            /*
            if (log_level <= AV_LOG_WARNING) {
              char buffer[256];
              vsprintf(buffer, format, args);
              std::cout << "AVCodec Issue: " << buffer;
            }
             */
          });
        });
#endif
        {
          std::lock_guard<std::mutex> av_lock(strong_this->_av_mutex);

          strong_this->_io_context_buffer = (unsigned char *)av_malloc(avio_context_buffer_size);
          strong_this->_io_context = avio_alloc_context(strong_this->_io_context_buffer,
                                                        avio_context_buffer_size,
                                                        0,
                                                        strong_this.get(),
                                                        &DecoderAVCodecImplementation::avio_read,
                                                        nullptr,
                                                        &DecoderAVCodecImplementation::avio_seek);
          strong_this->_format_context = avformat_alloc_context();
          strong_this->_resample_context = avresample_alloc_context();
          strong_this->_format_context->pb = strong_this->_io_context;

          int error_code = avformat_open_input(&strong_this->_format_context, "", nullptr, nullptr);
          if (error_code != 0) {
            decoder_error_callback(strong_this->name(), error_code);
            decoder_load_callback(false);
            return;
          }

          error_code = avformat_find_stream_info(strong_this->_format_context, nullptr);
          if (error_code != 0) {
            decoder_error_callback(strong_this->name(), error_code);
            decoder_load_callback(false);
            return;
          }
          bool format_found = false;
          for (int i = 0; i < strong_this->_format_context->nb_streams; ++i) {
            strong_this->_stream = strong_this->_format_context->streams[i];
            strong_this->_codec_context = strong_this->_stream->codec;
            if (strong_this->_codec_context->codec_type == AVMEDIA_TYPE_AUDIO) {
              auto frames = strong_this->_stream->nb_frames;
              auto duration_seconds =
                  static_cast<double>(strong_this->_stream->duration) /
                  (strong_this->_stream->time_base.den / strong_this->_stream->time_base.num);
              decltype(frames) duration_frames =
                  duration_seconds * strong_this->_codec_context->sample_rate;
              if (frames > 0 || duration_frames > 0) {
                strong_this->_frames = std::max(frames, duration_frames);
              } else {
                strong_this->_frames = UNKNOWN_FRAMES;
              }

              AVCodec *codec = avcodec_find_decoder(strong_this->_codec_context->codec_id);
              if (codec->id == AV_CODEC_ID_AAC) {
                strong_this->_start_junk_frames = 1024;
              } else if (codec->id == AV_CODEC_ID_MP3) {
                strong_this->_start_junk_frames = 275;
              }
              if (strong_this->_frames != UNKNOWN_FRAMES) {
                strong_this->_frames -= strong_this->_start_junk_frames;
              }
              error_code = avcodec_open2(strong_this->_codec_context, codec, nullptr);
              if (error_code != 0) {
                decoder_error_callback(strong_this->name(), error_code);
                decoder_load_callback(false);
                return;
              }

              if (strong_this->_decrypter) {
                // We need this to "guess" the IV's for decryption
                if (strong_this->_stream->nb_index_entries >= 2) {
                  AVIndexEntry &entry = strong_this->_stream->index_entries[0];
                  AVIndexEntry &entry2 = strong_this->_stream->index_entries[1];
                  strong_this->_frames_per_entry_index = entry2.timestamp - entry.timestamp;
                  strong_this->_packets_per_moof = strong_this->_stream->nb_index_entries;
                }
              }

              format_found = true;
              break;
            }
          }

          if (!format_found) {
            decoder_error_callback(strong_this->name(), ErrorCodeCouldNotDecodeHeader);
            decoder_load_callback(false);
            return;
          }

          av_opt_set_int(strong_this->_resample_context,
                         "in_channel_layout",
                         strong_this->_codec_context->channel_layout,
                         0);
          av_opt_set_int(
              strong_this->_resample_context, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
          av_opt_set_int(strong_this->_resample_context,
                         "in_sample_rate",
                         strong_this->_codec_context->sample_rate,
                         0);
          av_opt_set_int(
              strong_this->_resample_context, "out_sample_rate", strong_this->sampleRate(), 0);
          av_opt_set_int(strong_this->_resample_context,
                         "in_sample_fmt",
                         strong_this->_codec_context->sample_fmt,
                         0);
          av_opt_set_int(strong_this->_resample_context, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
          error_code = avresample_open(strong_this->_resample_context);
          if (error_code != 0) {
            decoder_error_callback(strong_this->name(), error_code);
            decoder_load_callback(false);
            return;
          }
        }
        decoder_load_callback(true);
      });
}

double DecoderAVCodecImplementation::sampleRate() {
  return 44100.0;
}

int DecoderAVCodecImplementation::channels() {
  return 2;
}

long DecoderAVCodecImplementation::currentFrameIndex() {
  return _frame_index;
}

void DecoderAVCodecImplementation::seek(long frame_index) {
  std::lock_guard<std::mutex> av_lock(_av_mutex);
  auto seek_frame_index = frame_index + _start_junk_frames;
  av_seek_frame(
      _format_context,
      _stream->index,
      (static_cast<double>(seek_frame_index) / static_cast<double>(_codec_context->sample_rate)) *
          (_stream->time_base.den / _stream->time_base.num),
      AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
  _pcm_buffer.clear();
  _frame_index = frame_index;
}

long DecoderAVCodecImplementation::frames() {
  return _frames;
}

void DecoderAVCodecImplementation::decode(long frames,
                                          const DECODE_CALLBACK &decode_callback,
                                          bool synchronous) {
  auto strong_this = shared_from_this();
  if (synchronous) {
    strong_this->runDecodeThread(frames, decode_callback);
  } else {
    std::thread([strong_this, decode_callback, frames] {
      strong_this->runDecodeThread(frames, decode_callback);
    }).detach();
  }
}

void DecoderAVCodecImplementation::runDecodeThread(long frames,
                                                   const DECODE_CALLBACK &decode_callback) {
  long frame_index = currentFrameIndex();
  int c = channels();
  long read_frames = 0l;
  float *output_samples = (float *)malloc(sizeof(float) * frames * c);
  {
    std::lock_guard<std::mutex> lock(_av_mutex);
    auto move_decoded = [&]() {
      auto frames_to_read = frames - read_frames;
      long samples_to_copy = std::min(static_cast<long>(_pcm_buffer.size()), frames_to_read * c);
      if (samples_to_copy == 0) {
        return;
      }
      for (long i = (read_frames * c), j = 0; i < (read_frames * c) + samples_to_copy; ++i, ++j) {
        output_samples[i] = _pcm_buffer[j];
      }
      _pcm_buffer.erase(_pcm_buffer.begin(), _pcm_buffer.begin() + samples_to_copy);
      read_frames += samples_to_copy / c;
    };
    move_decoded();
    bool drain = false;
    bool first_run = false;
    long decoded_frames = 0l;
    while (read_frames < frames) {
      // Decode the packet
      AVPacket *packet = av_packet_alloc();
      int error_code = av_read_frame(_format_context, packet);
      if (error_code == AVERROR_EOF) {
        // printf("HIT EOF calling av_read_frame, read_frames = %ld\n",
        // read_frames);
        drain = true;
      } else if (error_code == AVERROR(EAGAIN)) {
        // printf("NEED MORE INPUT (av_read_frame)\n");
        av_packet_free(&packet);
        continue;
      } else if (error_code != 0) {
        av_packet_free(&packet);
        continue;
      }
      AVPacket *p = drain ? nullptr : packet;

      if (_decrypter && p != nullptr && av_buffer_is_writable(p->buf)) {
        // Decrypt here?
        static const int INVALID_ENTRY_INDEX = -1;
        int entry_index = packet->pts / _frames_per_entry_index;
        if (entry_index != INVALID_ENTRY_INDEX && _ivs.find(entry_index) != _ivs.end()) {
          std::vector<unsigned char> input(p->buf->data, p->buf->data + p->buf->size);
          std::vector<unsigned char> output(p->buf->size, 0);
          unsigned char IV[16];
          memset(IV, 0, sizeof(IV));
          uint64_t new_iv;
          unsigned char *new_iv_bytes = (unsigned char *)&new_iv;
          uint64_t old_iv = _ivs[entry_index];
          unsigned char *old_iv_bytes = (unsigned char *)&old_iv;
          for (int i = 0; i < sizeof(uint64_t); ++i) {
            new_iv_bytes[i] = old_iv_bytes[sizeof(uint64_t) - i - 1];
          }
          memcpy(IV, new_iv_bytes, sizeof(new_iv));
          auto status = _decrypter->decrypt(input, output, _key_id, _key_id_length, IV, sizeof(IV));
          if (status == 0) {
            memcpy(p->buf->data, output.data(), p->buf->size);
          }
        }
      }
      error_code = avcodec_send_packet(_codec_context, p);

      if (error_code == AVERROR_EOF) {
        drain = true;
        av_packet_free(&packet);
        break;
      } else if (error_code != 0) {
        char err[1024];
        av_strerror(error_code, err, 1023);
        // printf("avcodec_send_packet failed: %s\n", err);
        av_packet_free(&packet);
        continue;
      }

      while (!error_code) {
        AVFrame *decoded_frame = av_frame_alloc();
        error_code = avcodec_receive_frame(_codec_context, decoded_frame);
        if (error_code == AVERROR_EOF) {
          // printf("EOF (avcodec_receive_frame)\n");
          av_frame_free(&decoded_frame);
          break;
        } else if (error_code == AVERROR(EAGAIN)) {
          // printf("NEED MORE INPUT (avcodec_receive_frame)\n");
          av_frame_free(&decoded_frame);
          break;
        } else if (error_code != 0) {
          // printf("avcodec_receive_frame failed\n");
          av_frame_free(&decoded_frame);
          continue;
        }

        if (decoded_frame->nb_samples <= 0) {
          av_frame_free(&decoded_frame);
          continue;
        }

        // Resample the audio
        auto samples_upper_bound =
            avresample_get_out_samples(_resample_context, decoded_frame->nb_samples);
        float **output_buffers = (float **)malloc(c * sizeof(float *));
        for (int i = 0; i < c; ++i) {
          output_buffers[i] = (float *)malloc(sizeof(float) * samples_upper_bound);
        }

        uint8_t *const *input_buffer = (uint8_t *const *)decoded_frame->extended_data;
        long pcm_frames = avresample_convert(_resample_context,
                                             (uint8_t **)output_buffers,
                                             0,
                                             samples_upper_bound,
                                             input_buffer,
                                             0,
                                             decoded_frame->nb_samples);

        if (pcm_frames <= 0) {
          for (int i = 0; i < c; ++i) {
            free(output_buffers[i]);
          }
          free(output_buffers);
          av_frame_free(&decoded_frame);
          continue;
        }

        // Place it into our PCM buffer
        int pcm_buffer_begin = _pcm_buffer.size();
        // FFMPEG seeks to the nearest packet, its up to us to clip that to the
        // nearest frame
        auto packet_seconds =
            static_cast<double>(p->pts) / (_stream->time_base.den / _stream->time_base.num);
        auto packet_frames = static_cast<long>(packet_seconds * _codec_context->sample_rate);
        long clip_frames = 0l;
        if (first_run && decoded_frames < _start_junk_frames && frame_index == 0) {
          clip_frames = _start_junk_frames - decoded_frames;
        } else if (!first_run) {
          clip_frames =
              std::max((frame_index + _start_junk_frames + read_frames) - packet_frames, 0l);
          first_run = true;
        }
        decoded_frames += pcm_frames;

        int samples = (pcm_frames * c) - (clip_frames * c);
        if (samples <= 0) {
          for (int i = 0; i < c; ++i) {
            free(output_buffers[i]);
          }
          free(output_buffers);
          av_frame_free(&decoded_frame);
          continue;
        }

        _pcm_buffer.insert(_pcm_buffer.end(), samples, 0.0f);
        for (int i = 0; i < pcm_frames; ++i) {
          if (i < clip_frames) {
            continue;
          }
          for (int j = 0; j < c; ++j) {
            auto adjusted_i = i - clip_frames;
            _pcm_buffer[pcm_buffer_begin + (adjusted_i * c) + j] = output_buffers[j][adjusted_i];
          }
        }
        for (int i = 0; i < c; ++i) {
          free(output_buffers[i]);
        }
        free(output_buffers);

        // Move it into our decoded buffer
        move_decoded();
        av_frame_free(&decoded_frame);
      }
      av_packet_free(&packet);
    }
    if (drain) {
      avcodec_flush_buffers(_codec_context);
    }
  }
  _frame_index = frame_index + read_frames;
  decode_callback(frame_index, std::min(read_frames, frames), output_samples);
  free(output_samples);
}

bool DecoderAVCodecImplementation::eof() {
  return _data_provider->eof();
}

const std::string &DecoderAVCodecImplementation::path() {
  return _data_provider->path();
}

void DecoderAVCodecImplementation::flush() {
  std::lock_guard<std::mutex> lock(_av_mutex);
  if (_codec_context != nullptr) {
    avcodec_flush_buffers(_codec_context);
  }
  if (_format_context != nullptr) {
    avformat_flush(_format_context);
  }
  if (_io_context != nullptr) {
    avio_flush(_io_context);
  }
  _pcm_buffer.clear();
}

int DecoderAVCodecImplementation::moofIndex(size_t byte_offset) {
  auto start_offset = _moofs.offset;
  auto i = 0;
  for (const auto &sidx : _moofs._sidx_frames) {
    auto end_offset = start_offset + sidx.referenced_size;
    if (byte_offset >= start_offset && byte_offset < end_offset) {
      return i;
    }
    start_offset = end_offset;
    ++i;
  }
  return INVALID_MOOF_INDEX;
}

int DecoderAVCodecImplementation::avio_read(void *opaque, uint8_t *buf, int buf_size) {
  DecoderAVCodecImplementation *decoder = (DecoderAVCodecImplementation *)opaque;

  int r = decoder->_data_provider->read(buf, 1, buf_size);

  // Here we do something very very bad... we search for SENC and TENC MPEG
  // boxes ourselves. This is because FFMPEG hides them from us and we need them
  // to decrypt content. May God forgive me, or let FFMPEG be a bit more usable
  // in the future
  if (decoder->_decrypter && r > 0) {
    std::vector<uint8_t> data(buf, buf + r);
    auto tell = decoder->_data_provider->tell();
    auto ensure_data_required = [&](size_t data_to_read, size_t offset) {
      if (data.size() >= data_to_read + offset) {
        return;
      }
      auto old_size = data.size();
      auto read_length = (data_to_read + offset) - old_size;
      data.insert(data.end(), read_length, 0);
      decoder->_data_provider->read(&data[old_size], read_length, sizeof(uint8_t));
    };
    if (!decoder->_found_sidx) {
      static const char SIDX[] = {'s', 'i', 'd', 'x'};
      static const char TENC[] = {'t', 'e', 'n', 'c'};
      for (int i = 0; i < data.size() - sizeof(SIDX); ++i) {
        unsigned char *tmp_buf = &data[i];
        if (memcmp(tmp_buf, SIDX, sizeof(SIDX)) == 0) {
          static const auto SIDX_FRAME_COUNT_BYTE_OFFSET = 22;
          // Read the number of MOOF's in the SIDX frame
          uint16_t moof_count = 0;
          const auto moof_count_offset = SIDX_FRAME_COUNT_BYTE_OFFSET + sizeof(SIDX);
          ensure_data_required(moof_count_offset, i);
          const auto moof_count_pointer = &data[moof_count_offset + i];
          memcpy(&moof_count, moof_count_pointer, sizeof(moof_count));
          swap_endians((uint8_t *)&moof_count, sizeof(decltype(moof_count)));
          // Read the SIDX frames
          auto sidx_frames_offset = moof_count_offset + sizeof(moof_count);
          auto required_sidx_frames_data = moof_count * sizeof(SIDX_FRAME);
          ensure_data_required(required_sidx_frames_data, i + sidx_frames_offset);
          decoder->_moofs.offset = sidx_frames_offset + (moof_count * sizeof(SIDX_FRAME));
          for (auto j = 0; j < moof_count; ++j) {
            SIDX_FRAME *sidx_frame =
                (SIDX_FRAME *)&tmp_buf[sidx_frames_offset + (j * sizeof(SIDX_FRAME))];
            swap_endians((uint8_t *)sidx_frame, sizeof(SIDX_FRAME));
            decoder->_moofs._sidx_frames.push_back(*sidx_frame);
          }
          decoder->_found_sidx = true;
        } else if (memcmp(tmp_buf, TENC, sizeof(TENC)) == 0) {
          static const auto TENC_KEY_ID_BYTE_OFFSET = 8;
          static const auto TENC_KEY_ID_SIZE = 16;
          ensure_data_required(TENC_KEY_ID_BYTE_OFFSET + TENC_KEY_ID_SIZE + sizeof(TENC), i);
          decoder->_key_id = (unsigned char *)malloc(TENC_KEY_ID_SIZE);
          memcpy(decoder->_key_id,
                 &data[i + TENC_KEY_ID_BYTE_OFFSET + sizeof(TENC)],
                 TENC_KEY_ID_SIZE);
          decoder->_key_id_length = TENC_KEY_ID_SIZE;
        }
      }
    }
    if (decoder->_found_sidx) {
      static const char MOOF[] = {'m', 'o', 'o', 'f'};
      for (int i = 0; i < data.size() - sizeof(MOOF); ++i) {
        uint8_t *tmp_buf = &data[i];
        if (memcmp(tmp_buf, MOOF, sizeof(MOOF)) == 0) {
          static const char SENC[] = {'s', 'e', 'n', 'c'};
          static const char TRUN[] = {'t', 'r', 'u', 'n'};
          auto current_moof_index = decoder->moofIndex(tell + i - r);
          auto moof_size = decoder->_moofs._sidx_frames[current_moof_index].referenced_size;
          ensure_data_required(moof_size, i);
          // Find the TRUN count
          short trun_count = 0;
          for (auto j = 0; j < moof_size - sizeof(SENC); ++j) {
            auto trun_buffer_byte_offset = i + j;
            ensure_data_required(trun_buffer_byte_offset, sizeof(TRUN));
            auto trun_buffer = &data[trun_buffer_byte_offset];
            if (memcmp(trun_buffer, TRUN, sizeof(TRUN)) == 0) {
              static const auto TRUN_COUNT_BYTE_OFFSET = sizeof(TRUN) + 6;
              ensure_data_required(TRUN_COUNT_BYTE_OFFSET + sizeof(decltype(trun_count)),
                                   trun_buffer_byte_offset);
              auto trun_count_offset = &data[trun_buffer_byte_offset + TRUN_COUNT_BYTE_OFFSET];
              memcpy(&trun_count, trun_count_offset, sizeof(trun_count));
              swap_endians((uint8_t *)&trun_count, sizeof(decltype(trun_count)));
            }
          }
          // Find the SENC
          for (auto j = 0; j < moof_size - sizeof(SENC); ++j) {
            auto senc_buffer_byte_offset = i + j;
            auto senc_buffer = &data[senc_buffer_byte_offset];
            if (memcmp(senc_buffer, SENC, sizeof(SENC)) == 0) {
              static const auto SENC_IVS_COUNT_BYTE_OFFSET = sizeof(SENC) + sizeof(uint32_t);
              // Read the number of IV's in the SENC
              auto entry_index = current_moof_index * decoder->_packets_per_moof;
              uint32_t ivs_count = 0;
              auto ivs_offset = SENC_IVS_COUNT_BYTE_OFFSET + sizeof(decltype(ivs_count));
              ensure_data_required(ivs_offset, senc_buffer_byte_offset);
              memcpy(&ivs_count,
                     &data[senc_buffer_byte_offset + SENC_IVS_COUNT_BYTE_OFFSET],
                     sizeof(ivs_count));
              swap_endians((uint8_t *)&ivs_count, sizeof(uint32_t));
              uint64_t iv = 0;
              ensure_data_required(ivs_offset + (sizeof(iv) * ivs_count), senc_buffer_byte_offset);
              for (decltype(ivs_count) k = 0; k < ivs_count; ++k) {
                auto iv_offset = &data[senc_buffer_byte_offset + SENC_IVS_COUNT_BYTE_OFFSET +
                                       sizeof(ivs_count) + (sizeof(iv) * k)];
                memcpy(&iv, iv_offset, sizeof(iv));
                swap_endians((uint8_t *)&iv, sizeof(iv));
                decoder->_ivs[entry_index++] = iv;
              }
            }
          }
        }
      }
    }
    if (decoder->_data_provider->tell() != tell) {
      decoder->_data_provider->seek(tell, SEEK_SET);
    }
  }

  return r;
}

int64_t DecoderAVCodecImplementation::avio_seek(void *opaque, int64_t offset, int whence) {
  DecoderAVCodecImplementation *decoder = (DecoderAVCodecImplementation *)opaque;
  if (whence == AVSEEK_SIZE) {
    return decoder->_data_provider->size();
  }
  return decoder->_data_provider->seek(offset, whence);
}

}  // namespace decoder
}  // namespace nativeformat

#endif
