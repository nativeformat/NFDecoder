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
#include "DecoderVorbisImplementation.h"

#include <cstdlib>
#include <future>

namespace nativeformat {
namespace decoder {

static const int VORBIS_READ_SIZE = 32768;  // Read 32kB at a time

const ov_callbacks DecoderVorbisImplementation::callbacks{
    .read_func = &DecoderVorbisImplementation::vorbis_read,
    .seek_func = &DecoderVorbisImplementation::vorbis_seek,
    .close_func = &DecoderVorbisImplementation::vorbis_close,
    .tell_func = &DecoderVorbisImplementation::vorbis_tell};

DecoderVorbisImplementation::DecoderVorbisImplementation(
    std::shared_ptr<DataProvider> &data_provider)
    : _data_provider(data_provider),
      _open(false),
      _info(nullptr),
      _channels(0),
      _samplerate(0.0),
      _frames(0),
      _frame_index(0),
      _current_section(0) {}

DecoderVorbisImplementation::~DecoderVorbisImplementation() {
  if (_open) {
    ov_clear(&_vorbis_file);
  }
}

const std::string &DecoderVorbisImplementation::name() {
  static const std::string domain("com.nativeformat.decoder.vorbis");
  return domain;
}

bool DecoderVorbisImplementation::checkCodec() {
  std::lock_guard<std::mutex> vorbis_lock(_vorbis_mutex);
  int error_code = ov_test_callbacks(this, &_vorbis_file, nullptr, 0, callbacks);
  if (error_code) {
    return false;
  }
  // vorbis supports passing initial data for file creation, so do that now
  error_code = ov_test_open(&_vorbis_file);
  if (error_code) {
    printf("Could not open vorbis file: %s\n", vorbis_error(error_code).c_str());
    return false;
  }
  ov_set_read_size(&_vorbis_file, VORBIS_READ_SIZE);
  _open = true;
  return true;
}

void DecoderVorbisImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                       const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  std::shared_ptr<DecoderVorbisImplementation> strong_this = shared_from_this();
  _load_future = std::async(
      std::launch::async, [strong_this, decoder_error_callback, decoder_load_callback]() {
        {
          std::lock_guard<std::mutex> vorbis_lock(strong_this->_vorbis_mutex);

          // Open file if checkCodec was not previously called
          if (!strong_this->_open) {
            int error_code = ov_open_callbacks(
                strong_this.get(), &strong_this->_vorbis_file, nullptr, 0, callbacks);
            if (error_code) {
              printf("Could not open vorbis file: %s\n", vorbis_error(error_code).c_str());
              decoder_error_callback(strong_this->name(), ErrorCodeCouldNotDecode);
              decoder_load_callback(false);
              return;
            }
            ov_set_read_size(&strong_this->_vorbis_file, VORBIS_READ_SIZE);
            strong_this->_open = true;
          }

          // Retrieve the header information
          strong_this->_info = ov_info(&strong_this->_vorbis_file, 0);
          if (strong_this->_info == nullptr) {
            decoder_error_callback(strong_this->name(), ErrorCodeCouldNotDecode);
            decoder_load_callback(false);
            return;
          }

          // Parse the information
          strong_this->_channels = strong_this->_info->channels;
          strong_this->_samplerate = static_cast<double>(strong_this->_info->rate);
          double time_total = ov_time_total(&strong_this->_vorbis_file, -1);
          strong_this->_frames = time_total * strong_this->sampleRate();
        }
        decoder_load_callback(true);
      });
}

double DecoderVorbisImplementation::sampleRate() {
  return _samplerate;
}

int DecoderVorbisImplementation::channels() {
  return _channels;
}

long DecoderVorbisImplementation::currentFrameIndex() {
  return _frame_index;
}

void DecoderVorbisImplementation::seek(long frame_index) {
  std::lock_guard<std::mutex> vorbis_lock(_vorbis_mutex);
  int error_code = ov_pcm_seek(&_vorbis_file, frame_index);
  if (error_code != 0) {
    return;
  }
  _frame_index = frame_index;
}

long DecoderVorbisImplementation::frames() {
  return _frames;
}

void DecoderVorbisImplementation::decode(long frames,
                                         const DECODE_CALLBACK &decode_callback,
                                         bool synchronous) {
  std::shared_ptr<DecoderVorbisImplementation> strong_this = shared_from_this();
  auto run_thread = [strong_this, decode_callback, frames] {
    long frame_index = strong_this->currentFrameIndex();

    // Make sure vorbis file is on the same page
    strong_this->seek(frame_index);

    int channels = strong_this->channels();
    float *interleaved_samples = (float *)malloc(frames * channels * sizeof(float));
    long read_frames = 0;
    {
      std::lock_guard<std::mutex> vorbis_lock(strong_this->_vorbis_mutex);
      while (read_frames < frames) {
        float **samples = nullptr;
        long current_read_frames = ov_read_float(&strong_this->_vorbis_file,
                                                 &samples,
                                                 frames - read_frames,
                                                 &strong_this->_current_section);
        if (current_read_frames <= 0) {
          if (current_read_frames == OV_HOLE) {
            continue;
          }
          if (current_read_frames < 0) {
            printf("Vorbis Error: %ld\n", current_read_frames);
          }
          break;
        }
        for (long i = 0; i < current_read_frames; ++i) {
          for (int j = 0; j < channels; ++j) {
            interleaved_samples[(i * channels) + j + (read_frames * channels)] = samples[j][i];
          }
        }
        read_frames += current_read_frames;
      }
    }

    strong_this->_frame_index = frame_index + read_frames;
    decode_callback(frame_index, read_frames, interleaved_samples);
    free(interleaved_samples);
  };
  if (synchronous) {
    run_thread();
  } else {
    std::thread(run_thread).detach();
  }
}

bool DecoderVorbisImplementation::eof() {
  return _data_provider->eof();
}

const std::string &DecoderVorbisImplementation::path() {
  return _data_provider->path();
}

void DecoderVorbisImplementation::flush() {}

size_t DecoderVorbisImplementation::vorbis_read(void *ptr,
                                                size_t size,
                                                size_t nmemb,
                                                void *datasource) {
  DecoderVorbisImplementation *decoder = (DecoderVorbisImplementation *)datasource;
  return decoder->_data_provider->read(ptr, size, nmemb);
}

int DecoderVorbisImplementation::vorbis_seek(void *datasource, ogg_int64_t offset, int whence) {
  DecoderVorbisImplementation *decoder = (DecoderVorbisImplementation *)datasource;
  return decoder->_data_provider->seek(offset, whence);
}

int DecoderVorbisImplementation::vorbis_close(void *datasource) {
  return 0;
}

long DecoderVorbisImplementation::vorbis_tell(void *datasource) {
  DecoderVorbisImplementation *decoder = (DecoderVorbisImplementation *)datasource;
  return decoder->_data_provider->tell();
}

std::string DecoderVorbisImplementation::vorbis_error(int code) {
  switch (code) {
    case 0:
      return "No error";
    case OV_FALSE:
      return "Not true, or no data available";
    case OV_EREAD:
      return "Read error while fetching compressed data for decode";
    case OV_EFAULT:
      return "Internal inconsistency in encode or decode state. Continuing is "
             "likely not possible.";
    case OV_EIMPL:
      return "Feature not implemented";
    case OV_EINVAL:
      return "Either an invalid argument, or incompletely initialized argument "
             "passed to a call";
    case OV_ENOTVORBIS:
      return "The given file/data was not recognized as Ogg Vorbis data.";
    case OV_EBADHEADER:
      return "The file/data is apparently an Ogg Vorbis stream, but contains a "
             "corrupted or undecipherable header.";
    case OV_EVERSION:
      return "The bitstream format revision of the given stream is not "
             "supported.";
    case OV_EBADLINK:
      return "The given link exists in the Vorbis data stream, but is not "
             "decipherable due to garbacge or corruption.";
    case OV_ENOSEEK:
      return "The given stream is not seekable";
    default:
      return "Other vorbis error";
  }
}

}  // namespace decoder
}  // namespace nativeformat
