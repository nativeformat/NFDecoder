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
#include "DecoderAudioConverterImplementation.h"

#if __APPLE__

#include <cstdlib>
#include <future>

namespace nativeformat {
namespace decoder {

DecoderAudioConverterImplementation::DecoderAudioConverterImplementation(
    std::shared_ptr<DataProvider> &data_provider)
    : _data_provider(data_provider), _frame_offset(0), _start_junk_frames(0) {}

DecoderAudioConverterImplementation::~DecoderAudioConverterImplementation() {
  AudioFileStreamClose(_audio_file_stream);
  if (_audio_converter_setup_complete) {
    AudioConverterDispose(_audio_converter);
  }
}

const std::string &DecoderAudioConverterImplementation::name() {
  return DECODER_AUDIOCONVERTER_NAME;
}

void DecoderAudioConverterImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                               const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  auto strong_this = shared_from_this();
  _load_future = std::async(
      std::launch::async, [strong_this, decoder_error_callback, decoder_load_callback]() {
        static const size_t maximum_header_size = 1024 * 1024;  // 1 MB

        {
          std::lock_guard<std::mutex> lock(strong_this->_audiotoolbox_mutex);
          AudioFileStreamOpen(strong_this.get(),
                              &DecoderAudioConverterImplementation::listenerProc,
                              &DecoderAudioConverterImplementation::sampleProc,
                              0,
                              &strong_this->_audio_file_stream);
          size_t read_bytes = 0;
          while (read_bytes < maximum_header_size) {
            // Read data from our provider
            static const size_t chunk_size = 1024 * 500;  // 500 KB
            unsigned char *buffer = (unsigned char *)malloc(chunk_size);
            size_t chunk_read_bytes =
                strong_this->_data_provider->read(buffer, sizeof(unsigned char), chunk_size);
            if (chunk_read_bytes == 0) {
              decoder_error_callback(strong_this->name(), ErrorCodeNotEnoughDataForHeader);
              decoder_load_callback(false);
              break;
            }

            // Parse the bytes we've got into the streamer
            OSStatus status = AudioFileStreamParseBytes(strong_this->_audio_file_stream,
                                                        chunk_read_bytes,
                                                        buffer,
                                                        (AudioFileStreamParseFlags)0);
            if (status > 0) {
              decoder_error_callback(strong_this->name(), status);
              decoder_load_callback(false);
              return;
            }
            free(buffer);

            // Check if we are ready to product packets
            UInt32 readyToProducePackets = 0;
            UInt32 readyToProducePacketsSize = sizeof(readyToProducePackets);
            AudioFileStreamGetProperty(strong_this->_audio_file_stream,
                                       kAudioFileStreamProperty_ReadyToProducePackets,
                                       &readyToProducePacketsSize,
                                       &readyToProducePackets);
            if (readyToProducePackets != 0) {
              // Setup the audio converter
              AudioStreamBasicDescription input_format;
              UInt32 descriptionSize = sizeof(input_format);
              OSStatus status = AudioFileStreamGetProperty(strong_this->_audio_file_stream,
                                                           kAudioFileStreamProperty_DataFormat,
                                                           &descriptionSize,
                                                           &input_format);
              if (status > 0) {
                decoder_error_callback(strong_this->name(), status);
                decoder_load_callback(false);
                return;
              }

              UInt32 audioPacketCount = 0;
              UInt32 audioPacketCountSize = sizeof(audioPacketCount);
              status = AudioFileStreamGetProperty(strong_this->_audio_file_stream,
                                                  kAudioFileStreamProperty_AudioDataPacketCount,
                                                  &audioPacketCountSize,
                                                  &audioPacketCount);
              if (status > 0) {
                if (status != kAudioFileStreamError_ValueUnknown) {
                  decoder_error_callback(strong_this->name(), status);
                  decoder_load_callback(false);
                  return;
                } else {
                  // Okej... let's estimate duration
                  UInt32 average_bytes_per_packet = 0;
                  UInt32 average_bytes_per_packet_size = sizeof(average_bytes_per_packet);
                  status =
                      AudioFileStreamGetProperty(strong_this->_audio_file_stream,
                                                 kAudioFileStreamProperty_AverageBytesPerPacket,
                                                 &average_bytes_per_packet_size,
                                                 &average_bytes_per_packet);
                  if (average_bytes_per_packet == 0) {
                    // You know Apple you are fucking amazing, I'll do this myself
                    // how 'bout that?
                    SInt64 cumulated_offset = 0;
                    SInt64 average_packet_count = 50;
                    SInt64 last_byte_offset = 0;
                    for (SInt64 i = 0; i < average_packet_count; ++i) {
                      SInt64 byte_offset = 0;
                      UInt32 io_flags = 0;
                      status = AudioFileStreamSeek(
                          strong_this->_audio_file_stream, i, &byte_offset, &io_flags);
                      if (status != noErr) {
                        decoder_error_callback(strong_this->name(), status);
                        decoder_load_callback(false);
                        return;
                      }
                      cumulated_offset += byte_offset - last_byte_offset;
                      last_byte_offset = byte_offset;
                    }
                    average_bytes_per_packet = cumulated_offset / average_packet_count;
                    SInt64 byte_offset = 0;
                    UInt32 io_flags = 0;
                    status = AudioFileStreamSeek(
                        strong_this->_audio_file_stream, 0, &byte_offset, &io_flags);
                  }
                  strong_this->_frames =
                      input_format.mFramesPerPacket *
                      (strong_this->_data_provider->size() / average_bytes_per_packet);
                }
              } else {
                strong_this->_frames = input_format.mFramesPerPacket * audioPacketCount;
              }
              strong_this->_data_provider->seek(0, SEEK_SET);

              strong_this->_channels = input_format.mChannelsPerFrame;
              strong_this->_samplerate = input_format.mSampleRate;

              strong_this->_output_format.mSampleRate = input_format.mSampleRate;
              strong_this->_output_format.mFormatID = kAudioFormatLinearPCM;
              strong_this->_output_format.mFormatFlags =
                  kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsPacked;
              strong_this->_output_format.mChannelsPerFrame = input_format.mChannelsPerFrame;
              strong_this->_output_format.mBytesPerFrame =
                  sizeof(float) * strong_this->_output_format.mChannelsPerFrame;
              strong_this->_output_format.mFramesPerPacket = 1;
              strong_this->_output_format.mBytesPerPacket =
                  strong_this->_output_format.mBytesPerFrame *
                  strong_this->_output_format.mFramesPerPacket;
              strong_this->_output_format.mBitsPerChannel = sizeof(float) * 8;

              status = AudioConverterNew(
                  &input_format, &strong_this->_output_format, &strong_this->_audio_converter);
              if (status > 0) {
                decoder_error_callback(strong_this->name(), status);
                decoder_load_callback(false);
                return;
              }

              // AAC encoders on Apple add 1024 junk frames to beginning of a decode
              // Except if we are under the transmuxers influence (memory data
              // provider)
              if (input_format.mFormatID == kAudioFormatMPEG4AAC &&
                  strong_this->_data_provider->name() != DATA_PROVIDER_MEMORY_NAME) {
                strong_this->_start_junk_frames = 1024;
              }
              if (strong_this->_frames > 0) {
                strong_this->_frames -= strong_this->_start_junk_frames;
              }
              strong_this->_audio_converter_setup_complete = true;

              break;
            }

            read_bytes += chunk_read_bytes;
          }
        }

        strong_this->seek(0);
        strong_this->_pcm_buffer.clear();
        decoder_load_callback(true);
      });
}

double DecoderAudioConverterImplementation::sampleRate() {
  return _samplerate;
}

int DecoderAudioConverterImplementation::channels() {
  return _channels;
}

long DecoderAudioConverterImplementation::currentFrameIndex() {
  return _frame_index;
}

void DecoderAudioConverterImplementation::seek(long frame_index) {
  std::lock_guard<std::mutex> lock(_audiotoolbox_mutex);
  AudioStreamBasicDescription input_format;
  UInt32 descriptionSize = sizeof(input_format);
  OSStatus status = AudioFileStreamGetProperty(
      _audio_file_stream, kAudioFileStreamProperty_DataFormat, &descriptionSize, &input_format);
  if (status < 0) {
    return;
  }
  SInt64 packetOffset = frame_index / input_format.mFramesPerPacket;
  long frame_offset = frame_index % input_format.mFramesPerPacket;
  SInt64 dataByteOffset = 0;
  AudioFileStreamSeekFlags flags;
  status = AudioFileStreamSeek(_audio_file_stream, packetOffset, &dataByteOffset, &flags);
  if (status < 0) {
    return;
  }

  if (packetOffset == 0) {
    UInt32 data_offset_size = sizeof(dataByteOffset);
    status = AudioFileStreamGetProperty(_audio_file_stream,
                                        kAudioFileStreamProperty_DataOffset,
                                        &data_offset_size,
                                        &dataByteOffset);
    if (status < 0) {
      return;
    }
  }

  _frame_offset = frame_offset;
  _frame_index = frame_index;
  _pcm_buffer.clear();
  _data_provider->seek(dataByteOffset, SEEK_SET);
}

long DecoderAudioConverterImplementation::frames() {
  return _frames;
}

void DecoderAudioConverterImplementation::decode(long frames,
                                                 const DECODE_CALLBACK &decode_callback,
                                                 bool synchronous) {
  auto strong_this = shared_from_this();
  auto run_thread = [strong_this, decode_callback, frames] {
    long frame_index = strong_this->currentFrameIndex();
    int channels = strong_this->channels();
    float *samples = (float *)malloc(frames * channels * sizeof(float));
    long read_frames = 0;
    {
      std::lock_guard<std::mutex> lock(strong_this->_audiotoolbox_mutex);
      auto fill_data = [&]() -> OSStatus {
        size_t data_size = 1024 * 500;  // 500 kB
        unsigned char *data = (unsigned char *)malloc(data_size);
        size_t read_data =
            strong_this->_data_provider->read(data, sizeof(unsigned char), data_size);
        OSStatus status = noErr;
        if (read_data == 0) {
          status = -1;
        } else {
          status = AudioFileStreamParseBytes(
              strong_this->_audio_file_stream, read_data, data, (AudioFileStreamParseFlags)0);
        }
        free(data);
        return status;
      };
      auto dump_data = [&]() {
        long frames_to_read = frames - read_frames;
        auto current_frame_index = frame_index + read_frames;
        if (current_frame_index < strong_this->_start_junk_frames) {
          auto delete_frames = std::min(
              strong_this->_start_junk_frames - current_frame_index,
              static_cast<long>(strong_this->_pcm_buffer.size() / strong_this->channels()));
          strong_this->_pcm_buffer.erase(
              strong_this->_pcm_buffer.begin(),
              strong_this->_pcm_buffer.begin() + (delete_frames * strong_this->channels()));
          frames_to_read -= delete_frames;
        }
        long pcm_samples = strong_this->_pcm_buffer.size();
        if (frames_to_read > 0 && pcm_samples > 0) {
          long samples_to_read = std::min((long)pcm_samples, frames_to_read * channels);
          memcpy(&samples[read_frames * channels],
                 &strong_this->_pcm_buffer[0],
                 samples_to_read * sizeof(float));
          strong_this->_pcm_buffer.erase(strong_this->_pcm_buffer.begin(),
                                         strong_this->_pcm_buffer.begin() + samples_to_read);
          read_frames += samples_to_read / channels;
        }
      };
      while (read_frames < frames) {
        if (strong_this->_frame_offset > 0) {
          long maximum_erase =
              std::min(static_cast<long>(strong_this->_pcm_buffer.size() / channels),
                       strong_this->_frame_offset);
          strong_this->_pcm_buffer.erase(
              strong_this->_pcm_buffer.begin(),
              strong_this->_pcm_buffer.begin() + (maximum_erase * channels));
          strong_this->_frame_offset -= maximum_erase;
          if (strong_this->_frame_offset > 0) {
            if (fill_data() != noErr) {
              break;
            }
          }
        } else {
          if (strong_this->_pcm_buffer.empty()) {
            if (fill_data() != noErr) {
              break;
            }
          } else {
            dump_data();
          }
        }
      }
      dump_data();
    }
    decode_callback(frame_index, read_frames, samples);
    free(samples);
    strong_this->_frame_index = frame_index + read_frames;
  };
  if (synchronous) {
    run_thread();
  } else {
    std::thread(run_thread).detach();
  }
}

bool DecoderAudioConverterImplementation::eof() {
  return _data_provider->eof();
}

const std::string &DecoderAudioConverterImplementation::path() {
  return _data_provider->path();
}

void DecoderAudioConverterImplementation::flush() {
  std::lock_guard<std::mutex> lock(_audiotoolbox_mutex);
  _pcm_buffer.clear();
  AudioConverterReset(_audio_converter);
}

void DecoderAudioConverterImplementation::listenerProc(void *inClientData,
                                                       AudioFileStreamID inAudioFileStream,
                                                       AudioFileStreamPropertyID inPropertyID,
                                                       AudioFileStreamPropertyFlags *ioFlags) {}

void DecoderAudioConverterImplementation::sampleProc(
    void *inClientData,
    UInt32 inNumberBytes,
    UInt32 inNumberPackets,
    const void *inInputData,
    AudioStreamPacketDescription *inPacketDescriptions) {
  DecoderAudioConverterImplementation *decoder =
      (DecoderAudioConverterImplementation *)inClientData;

  UInt32 maximum_output_packet_size = 0;
  UInt32 maximum_output_packet_size_property = sizeof(maximum_output_packet_size);
  OSStatus status = AudioConverterGetProperty(decoder->_audio_converter,
                                              kAudioConverterPropertyMaximumOutputPacketSize,
                                              &maximum_output_packet_size_property,
                                              &maximum_output_packet_size);

  void *input_data_copy = malloc(inNumberBytes);
  memcpy(input_data_copy, inInputData, inNumberBytes);
  decoder->_input_buffer.mData = input_data_copy;
  decoder->_input_buffer.mDataByteSize = inNumberBytes;

  void *output_buffer = calloc(maximum_output_packet_size, decoder->_output_format.mBytesPerPacket);
  AudioBufferList output_buffer_list;
  output_buffer_list.mNumberBuffers = 1;
  output_buffer_list.mBuffers[0].mNumberChannels = decoder->_output_format.mChannelsPerFrame;
  output_buffer_list.mBuffers[0].mDataByteSize =
      maximum_output_packet_size * decoder->_output_format.mBytesPerPacket;
  output_buffer_list.mBuffers[0].mData = output_buffer;

  decoder->_input_packets = inNumberPackets;
  while (status == noErr) {
    UInt32 output_data_packet_size = maximum_output_packet_size;
    decoder->_packet_descriptions = inPacketDescriptions;
    status = AudioConverterFillComplexBuffer(decoder->_audio_converter,
                                             &DecoderAudioConverterImplementation::inputDataProc,
                                             inClientData,
                                             &output_data_packet_size,
                                             &output_buffer_list,
                                             nil);
    if (status != noErr || output_data_packet_size == 0) {
      break;
    }

    UInt32 number_frames_produced_in_buffer =
        output_data_packet_size * decoder->_output_format.mFramesPerPacket;
    UInt32 output_data_bytes =
        number_frames_produced_in_buffer * decoder->_output_format.mBytesPerFrame;
    float *output_data = (float *)output_buffer_list.mBuffers[0].mData;
    UInt32 output_samples = output_data_bytes / sizeof(float);
    decoder->_pcm_buffer.insert(
        decoder->_pcm_buffer.end(), output_data, output_data + output_samples);
  }

  free(output_buffer);
  free(input_data_copy);
}

OSStatus DecoderAudioConverterImplementation::inputDataProc(
    AudioConverterRef inAudioConverter,
    UInt32 *ioNumberDataPackets,
    AudioBufferList *ioData,
    AudioStreamPacketDescription **outDataPacketDescription,
    void *inUserData) {
  DecoderAudioConverterImplementation *decoder = (DecoderAudioConverterImplementation *)inUserData;

  if (*ioNumberDataPackets == 0 || decoder->_input_buffer.mData == NULL) {
    ioData->mBuffers[0].mDataByteSize = 0;
    ioData->mBuffers[0].mData = NULL;
    *ioNumberDataPackets = 0;
    return kAudio_ParamError;
  }

  // Calculate our input buffer
  ioData->mNumberBuffers = 1;
  ioData->mBuffers[0] = decoder->_input_buffer;
  decoder->_input_buffer.mData = NULL;
  decoder->_input_buffer.mDataByteSize = 0;

  // Output the packet descriptions
  *ioNumberDataPackets = decoder->_input_packets;
  *outDataPacketDescription = decoder->_packet_descriptions;

  return noErr;
}

}  // namespace decoder
}  // namespace nativeformat

#endif
