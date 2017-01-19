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

#include "DecoderWindowsImplementation.h"
#include <Propvarutil.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

// Requires: Mfuuid.lib

template <class T>
void SafeRelease(T **pT) {
  if (*pT) {
    (*pT)->Release();
    *pT = NULL;
  }
}

// Provides "bytesRead" for an asynchronous IMFByteStream read.
class AsynchronousReadRequest : public IUnknown {
 public:
  ULONG bytesRead;

  AsynchronousReadRequest(ULONG _bytesRead) : refCount(1), bytesRead(_bytesRead) {}

  ~AsynchronousReadRequest() {}

  STDMETHODIMP QueryInterface(REFIID iid, void **_interface) {
    if (!_interface) return E_INVALIDARG;
    *_interface = NULL;
    if (iid == IID_IUnknown) {
      *_interface = (LPVOID)this;
      AddRef();
      return NOERROR;
    } else
      return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&refCount); }

  STDMETHODIMP_(ULONG) Release() {
    ULONG r = InterlockedDecrement(&refCount);
    if (r == 0) delete this;
    return r;
  }

 private:
  ULONG refCount;
};

// The custom data provider for IMFSourceReader, wrapping DataProvider.
class NFByteStreamHandler : public IMFByteStream {
 public:
  NFByteStreamHandler(std::shared_ptr<nativeformat::decoder::DataProvider> _dataProvider)
      : dataProvider(_dataProvider), refCount(1), asyncRead(false) {}

  ~NFByteStreamHandler() {}

  // IUnknown implementation
  STDMETHODIMP QueryInterface(REFIID iid, LPVOID *_interface) {
    if (!_interface) return E_INVALIDARG;
    *_interface = NULL;
    if ((iid == IID_IUnknown) || (iid == IID_IMFByteStream)) {
      *_interface = (LPVOID)this;
      AddRef();
      return NOERROR;
    } else
      return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&refCount); }

  STDMETHODIMP_(ULONG) Release() {
    ULONG r = InterlockedDecrement(&refCount);
    if (r == 0) delete this;
    return r;
  }

  // IMFByteStream implementation
  STDMETHODIMP BeginRead(BYTE *buffer,
                         ULONG length,
                         IMFAsyncCallback *callback,
                         IUnknown *callerState) {
    asyncRead = true;
    ULONG bytesRead;
    Read(buffer, length, &bytesRead);

    AsynchronousReadRequest *request = new AsynchronousReadRequest(bytesRead);
    IMFAsyncResult *result;
    HRESULT hr = MFCreateAsyncResult(request, callback, callerState, &result);
    SafeRelease(&request);

    if (SUCCEEDED(hr)) {
      result->SetStatus(S_OK);
      hr = MFInvokeCallback(result);
    }
    return hr;
  }

  STDMETHODIMP EndRead(IMFAsyncResult *result, ULONG *bytesRead) {
    IUnknown *unknown;
    AsynchronousReadRequest *request;
    HRESULT hr = result->GetObject(&unknown);
    if (FAILED(hr) || !unknown) return E_INVALIDARG;
    request = static_cast<AsynchronousReadRequest *>(unknown);

    *bytesRead = request->bytesRead;
    SafeRelease(&request);
    asyncRead = false;

    hr = result->GetStatus();
    SafeRelease(&result);
    return hr;
  }

  STDMETHODIMP Read(BYTE *buffer, ULONG length, ULONG *bytesRead) {
    *bytesRead = (ULONG)dataProvider->read(buffer, 1, length);
    return S_OK;
  }

  STDMETHODIMP SetCurrentPosition(QWORD position) {
    QWORD currentPosition;
    return Seek(msoBegin, position, 0, &currentPosition);
  }

  STDMETHODIMP Seek(MFBYTESTREAM_SEEK_ORIGIN origin,
                    LONGLONG offset,
                    DWORD flags,
                    QWORD *currentPosition) {
    if (asyncRead) return E_INVALIDARG;
    if (origin == msoCurrent) offset += dataProvider->tell();
    dataProvider->seek((long)offset, SEEK_SET);
    *currentPosition = dataProvider->tell();
    return S_OK;
  }

  STDMETHODIMP GetCapabilities(DWORD *capabilities) {
    *capabilities = MFBYTESTREAM_IS_READABLE | MFBYTESTREAM_IS_SEEKABLE;
    return S_OK;
  }

  STDMETHODIMP GetCurrentPosition(QWORD *position) {
    *position = dataProvider->tell();
    return S_OK;
  }

  STDMETHODIMP GetLength(QWORD *length) {
    *length = dataProvider->size();
    return S_OK;
  }

  STDMETHODIMP IsEndOfStream(BOOL *isEndOfStream) {
    *isEndOfStream = dataProvider->eof();
    return S_OK;
  }

  // IMFByteStream not implemented
  STDMETHODIMP BeginWrite(const BYTE *buffer,
                          ULONG length,
                          IMFAsyncCallback *callback,
                          IUnknown *callerState) {
    return S_OK;
  }
  STDMETHODIMP Write(const BYTE *buffer, ULONG length, ULONG *bytesWritten) { return S_OK; }
  STDMETHODIMP EndWrite(IMFAsyncResult *result, ULONG *bytesWritten) { return S_OK; }
  STDMETHODIMP SetLength(QWORD length) { return S_OK; }
  STDMETHODIMP Flush() { return S_OK; }
  STDMETHODIMP Close() { return S_OK; }

 private:
  ULONG refCount;
  bool asyncRead;
  std::shared_ptr<nativeformat::decoder::DataProvider> dataProvider;
};

namespace nativeformat {
namespace decoder {

static const std::string domain("com.nativeformat.decoder.Windows");

typedef struct decoderWindowsInternals {
  std::shared_ptr<DataProvider> dataProvider;
  IMFSourceReader *sourceReader;
  NFByteStreamHandler *byteStreamHandler;
  float *floatBuffer;
  double samplerate;
  long durationFrames;
  int channels, bufferCapacityFrames, bufferReadPosFrames, bufferWritePosFrames, numFramesInBuffer,
      currentFrameIndex, insertSilenceFrames;
  bool eof, afterSeek;
} decoderWindowsInternals;

DecoderWindowsImplementation::DecoderWindowsImplementation(
    std::shared_ptr<DataProvider> dataProvider) {
  internals = new decoderWindowsInternals;
  memset(internals, 0, sizeof(decoderWindowsInternals));
  internals->dataProvider = dataProvider;
  internals->eof = false;
  internals->afterSeek = true;
  internals->bufferCapacityFrames = 4096;
}

DecoderWindowsImplementation::~DecoderWindowsImplementation() {
  if (internals->sourceReader) internals->sourceReader->Release();
  if (internals->byteStreamHandler) delete internals->byteStreamHandler;
  if (internals->floatBuffer) free(internals->floatBuffer);
  delete internals;
  MFShutdown();
}

void DecoderWindowsImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                        const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  internals->byteStreamHandler = new NFByteStreamHandler(internals->dataProvider);

  // Creating the Media Foundation reader.
  HRESULT hr = MFStartup(MF_VERSION);
  if (FAILED(hr)) {
    decoder_error_callback("MFStartup failed.", 0);
    decoder_load_callback(false);
    return;
  }
  hr = MFCreateSourceReaderFromByteStream(
      internals->byteStreamHandler, NULL, &internals->sourceReader);
  if (FAILED(hr)) {
    decoder_error_callback("MFCreateSourceReaderFromByteStream failed.", 0);
    decoder_load_callback(false);
    return;
  }

  // Setting up the reader to select the first audio stream and output PCM
  // audio.
  hr = internals->sourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
  if (SUCCEEDED(hr))
    hr = internals->sourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                                     TRUE);
  IMFMediaType *partialType = NULL;
  hr = MFCreateMediaType(&partialType);
  if (SUCCEEDED(hr)) hr = partialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  if (SUCCEEDED(hr)) hr = partialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
  if (SUCCEEDED(hr))
    hr = internals->sourceReader->SetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, partialType);
  if (SUCCEEDED(hr))
    hr = internals->sourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                                     TRUE);
  if (partialType) partialType->Release();
  if (FAILED(hr)) {
    decoder_error_callback("Configuring the audio stream failed.", 0);
    decoder_load_callback(false);
    return;
  }

  // Get important metrics.
  partialType = NULL;
  hr = internals->sourceReader->GetCurrentMediaType(0, &partialType);
  if (FAILED(hr)) {
    decoder_error_callback("Can't get the current media type.", 0);
    decoder_load_callback(false);
    return;
  }

  UINT32 uint = 0;
  hr = partialType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &uint);
  if (FAILED(hr)) {
    decoder_error_callback("Can't get the number of channels.", 0);
    decoder_load_callback(false);
    return;
  }
  internals->channels = uint;

  uint = 0;
  hr = partialType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &uint);
  if (FAILED(hr)) {
    decoder_error_callback("Can't get the sample rate.", 0);
    decoder_load_callback(false);
    return;
  }
  internals->samplerate = uint;

  PROPVARIANT var;
  hr = internals->sourceReader->GetPresentationAttribute(
      MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var);
  if (SUCCEEDED(hr)) {
    LONGLONG duration100ns = var.uhVal.QuadPart;
    double durationSeconds = duration100ns / 10000000.0;
    internals->durationFrames = long(durationSeconds * internals->samplerate);
  } else {
    decoder_error_callback("Can't get the duration.", 0);
    decoder_load_callback(false);
    return;
  }

  // Allocate the internal buffer.
  internals->floatBuffer =
      (float *)malloc(internals->channels * sizeof(float) * internals->bufferCapacityFrames);
  if (!internals->floatBuffer) {
    decoder_error_callback("Out of memory.", 0);
    decoder_load_callback(false);
    return;
  }

  decoder_load_callback(true);
}

// Returns false on out of memory, true otherwise.
static bool putIntoFloatBuffer(decoderWindowsInternals *internals,
                               short int *input,
                               int bytesRead) {
  int framesRead = bytesRead / (sizeof(short int) * internals->channels);

  // Do we have enough space for the new data?
  int bufferCapacityFrames = internals->bufferCapacityFrames - internals->bufferWritePosFrames;
  if (bufferCapacityFrames < framesRead) {
    // Try to wrap first.
    if (internals->numFramesInBuffer > 0)
      memmove(internals->floatBuffer,
              internals->floatBuffer + internals->bufferReadPosFrames * internals->channels,
              internals->numFramesInBuffer * internals->channels * sizeof(float));
    internals->bufferReadPosFrames = 0;
    internals->bufferWritePosFrames = internals->numFramesInBuffer;

    // If our buffer is still not big enough, realloc.
    bufferCapacityFrames = internals->bufferCapacityFrames - internals->bufferWritePosFrames;
    if (bufferCapacityFrames < framesRead) {
      int newCapacityFrames = internals->bufferCapacityFrames + framesRead * 2;
      float *newBuffer = (float *)realloc(internals->floatBuffer,
                                          newCapacityFrames * internals->channels * sizeof(float));
      if (newBuffer) {
        internals->floatBuffer = newBuffer;
        internals->bufferCapacityFrames = newCapacityFrames;
      } else
        return false;
    }
  }

  // Copy into our internal buffer.
  float *floats = internals->floatBuffer + internals->bufferWritePosFrames * internals->channels;
  int numSamples = framesRead * internals->channels;

  static const float multiplier = 1.0f / 32767.0f;  // Multiplying is much faster than dividing.
  while (numSamples--) *floats++ = float(*input++) * multiplier;

  internals->bufferWritePosFrames += framesRead;
  internals->numFramesInBuffer += framesRead;
  return true;
}

// Performs a block of IMFSourceReader decoding.
static int winDecode(decoderWindowsInternals *internals) {
  DWORD flags = 0;
  IMFSample *sample = NULL;
  LONGLONG timestamp = 0;
  HRESULT hr = internals->sourceReader->ReadSample(
      (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, &timestamp, &sample);

  if (FAILED(hr)) return -1;
  if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) return -1;
  if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
    internals->eof = true;
    return -1;
  }
  if (sample == NULL) return -1;

  // Get the buffer.
  IMFMediaBuffer *buffer = NULL;
  hr = sample->ConvertToContiguousBuffer(&buffer);
  if (FAILED(hr)) return -1;
  BYTE *audioData = NULL;
  DWORD bytes = 0;
  hr = buffer->Lock(&audioData, NULL, &bytes);
  if (FAILED(hr)) return -1;

  putIntoFloatBuffer(internals, (short int *)audioData, bytes);

  // Release the buffer.
  hr = buffer->Unlock();
  if (FAILED(hr)) return -1;
  sample->Release();
  buffer->Release();

  // Get the current position _before_ ReadSample.
  double timestampSeconds = timestamp / 10000000.0;
  int timestampFrames = int(timestampSeconds * internals->samplerate);
  return timestampFrames;
}

void DecoderWindowsImplementation::decode(long frames, const DECODE_CALLBACK &decode_callback) {
  // Creating the output buffer.
  float *outputBuffer = (float *)malloc(frames * internals->channels * sizeof(float));
  if (!outputBuffer) {
    decode_callback(internals->currentFrameIndex, 0, NULL);
    return;
  }

  long framesRead = 0;
  while (framesRead < frames) {
    // Insert silence if we have to.
    if (internals->insertSilenceFrames > 0) {
      int silenceFrames =
          (internals->insertSilenceFrames > frames) ? frames : internals->insertSilenceFrames;
      memset(outputBuffer, 0, silenceFrames * internals->channels * sizeof(float));
      internals->insertSilenceFrames -= silenceFrames;
      framesRead += silenceFrames;
      internals->currentFrameIndex += silenceFrames;
      frames -= silenceFrames;
      continue;
    }

    // Do we have some frames buffered that we can offer?
    long framesFromBuffer = internals->numFramesInBuffer;
    if (framesFromBuffer > frames) framesFromBuffer = frames;
    if (framesFromBuffer > 0) {
      memcpy(outputBuffer + framesRead * internals->channels,
             internals->floatBuffer + internals->bufferReadPosFrames * internals->channels,
             framesFromBuffer * internals->channels * sizeof(float));
      framesRead += framesFromBuffer;
      internals->currentFrameIndex += framesFromBuffer;
      internals->bufferReadPosFrames += framesFromBuffer;
      internals->numFramesInBuffer -= framesFromBuffer;
      frames -= framesFromBuffer;
      continue;
    }

    if (internals->eof) break;

    // Decode more frames.
    int timestampFrames = winDecode(internals);
    if (timestampFrames < 0) break;  // Error.

    // Cut or add frames after seeking.
    if (internals->afterSeek) {
      internals->afterSeek = false;

      int framesBuffered = internals->numFramesInBuffer;   // Number of frames buffered after the
                                                           // first ReadSample call.
      timestampFrames = winDecode(internals);              // Windows can only tell us the
                                                           // position before the call, which
                                                           // needs two ReadSample calls to
                                                           // be correct.
      if (timestampFrames < 0) break;                      // Error.
      timestampFrames = timestampFrames - framesBuffered;  // This is the true position.

      int framesToCut = internals->currentFrameIndex - timestampFrames;
      if (framesToCut < 0)
        internals->insertSilenceFrames = -framesToCut;  // Can happen when we seek to 0.
      else if (framesToCut > 0) {                       // Typical case after seek.
        internals->bufferReadPosFrames += framesToCut;
        internals->numFramesInBuffer -= framesToCut;
      }
    }
  }

  // Passing the result.
  decode_callback(internals->currentFrameIndex, framesRead, outputBuffer);
  free(outputBuffer);
}

void DecoderWindowsImplementation::seek(long frame_index) {
  if (internals->currentFrameIndex == frame_index) return;

  // The position needs to be in the format of 100 nanoseconds.
  double timestampSeconds = frame_index / internals->samplerate;
  PROPVARIANT var;
  var.vt = VT_I8;
  var.hVal.QuadPart = (LONGLONG)(timestampSeconds * 10000000.0);

  HRESULT hr = internals->sourceReader->SetCurrentPosition(GUID_NULL, var);
  if (SUCCEEDED(hr)) {
    internals->currentFrameIndex = (int)frame_index;
    internals->bufferWritePosFrames = internals->bufferReadPosFrames =
        internals->numFramesInBuffer = internals->insertSilenceFrames = 0;
    internals->eof = false;
    internals->afterSeek = true;
  }
}

void DecoderWindowsImplementation::flush() {
  internals->sourceReader->Flush(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
  internals->bufferWritePosFrames = internals->bufferReadPosFrames = internals->numFramesInBuffer =
      internals->insertSilenceFrames = 0;
  internals->eof = false;
}

double DecoderWindowsImplementation::sampleRate() {
  return internals->samplerate;
}

int DecoderWindowsImplementation::channels() {
  return internals->channels;
}

const std::string &DecoderWindowsImplementation::path() {
  return internals->dataProvider->path();
}

const std::string &DecoderWindowsImplementation::name() {
  return domain;
}

long DecoderWindowsImplementation::currentFrameIndex() {
  return internals->currentFrameIndex;
}

long DecoderWindowsImplementation::frames() {
  return internals->durationFrames;
}

bool DecoderWindowsImplementation::eof() {
  return internals->eof;
}

}  // namespace decoder
}  // namespace nativeformat
