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
#include "DecoderAndroidImplementation.h"

#if ANDROID

#include <jni.h>
#include <stdlib.h>
#include <string.h>

namespace nativeformat {
namespace decoder {

static JavaVM *javaVM = NULL;
static jclass decoderClass = NULL;
static jfieldID samplerateFieldID;
static jfieldID channelFieldID;
static jfieldID durationFieldID;
static jmethodID errorMethodID;
static jmethodID decodeMethodID;
static jmethodID seekMethodID;

// Java can not realloc our receiving buffer at all. We suppose that 512 kb
// should be enough for one packet.
#define JAVABUFFERSIZEBYTES (512 * 1024)

static const std::string domain("com.nativeformat.decoder.android");

typedef struct decoderAndroidInternals {
  std::shared_ptr<DataProvider> dataProvider;
  jobject javaDecoder;
  short int *intBuffer;
  float *floatBuffer;
  double samplerate;
  long durationFrames;
  int channels, bufferCapacityFrames, bufferReadPosFrames, bufferWritePosFrames, numFramesInBuffer,
      currentFrameIndex;
  bool eof;
} decoderAndroidInternals;

// Provides the actual source bytes.
static jint nativeReadAt(JNIEnv *env,
                         __unused jobject obj,
                         jlong clientdata,
                         jlong position,
                         jbyteArray buffer,
                         jint offset,
                         jint size) {
  decoderAndroidInternals *internals = (decoderAndroidInternals *)clientdata;
  if (internals->dataProvider->tell() != position)
    internals->dataProvider->seek(position, SEEK_SET);
  jbyte *buf = env->GetByteArrayElements(buffer, NULL);
  jint r = (jint)internals->dataProvider->read(buf + offset, 1, (size_t)size);
  env->ReleaseByteArrayElements(buffer, buf, 0);
  return r;
}

static jlong nativeGetSize(__unused JNIEnv *env, __unused jobject obj, jlong clientdata) {
  decoderAndroidInternals *internals = (decoderAndroidInternals *)clientdata;
  return internals->dataProvider->size();
}

static JNINativeMethod nativeMethods[] = {
    {"nativeReadAt", "(JJ[BII)I", (void *)nativeReadAt},
    {"nativeGetSize", "(J)J", (void *)nativeGetSize},
};

// Finds our Decoder class in Java.
const char *DecoderAndroidImplementation::initJavaOnAppLaunch(JNIEnv *javaEnvironment,
                                                              jobject activityObject) {
  if (javaEnvironment->GetJavaVM(&javaVM) != 0) return "Can't get Java VM.";

  // Get the class loader from the activity.
  jclass activityClass = javaEnvironment->GetObjectClass(activityObject);
  if (!activityClass) return "Can't get the activity's class.";
  jmethodID getClassLoader =
      javaEnvironment->GetMethodID(activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
  if (!getClassLoader) return "Can't get getClassLoader.";
  jobject cls = javaEnvironment->CallObjectMethod(activityObject, getClassLoader);
  if (!cls) return "Can't get cls.";
  jclass classLoader = javaEnvironment->FindClass("java/lang/ClassLoader");
  if (!classLoader) return "Can't get ClassLoader.";
  jmethodID findClass = javaEnvironment->GetMethodID(
      classLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
  if (!findClass) return "Can't get loadClass.";

  // Find our Decoder class.
  jstring javaClassName = javaEnvironment->NewStringUTF("com.spotify.NFDecoder.Decoder");
  decoderClass = (jclass)javaEnvironment->CallObjectMethod(cls, findClass, javaClassName);
  if (!decoderClass) return "Can't find the Decoder class.";
  decoderClass = (jclass)javaEnvironment->NewGlobalRef(decoderClass);
  if (!decoderClass) return "Can't make the Decoder class global.";

  // Get the methods and parameters of our Decoder class.
  samplerateFieldID = javaEnvironment->GetFieldID(decoderClass, "samplerate", "I");
  if (!samplerateFieldID) return "Can't find the samplerate field.";
  channelFieldID = javaEnvironment->GetFieldID(decoderClass, "numberOfChannels", "I");
  if (!channelFieldID) return "Can't find the numberOfChannels field.";
  durationFieldID = javaEnvironment->GetFieldID(decoderClass, "durationFrames", "J");
  if (!durationFieldID) return "Can't find the durationFrames field.";
  errorMethodID =
      javaEnvironment->GetMethodID(decoderClass, "getLastError", "()Ljava/lang/String;");
  if (!errorMethodID) return "Can't find the getLastError method.";
  decodeMethodID =
      javaEnvironment->GetMethodID(decoderClass, "decodeOnePacket", "(Ljava/nio/ByteBuffer;)I");
  if (!decodeMethodID) return "Can't find the decode method.";
  seekMethodID = javaEnvironment->GetMethodID(decoderClass, "seek", "(I)Z");
  if (!seekMethodID) return "Can't find the seek method.";

  // Register the native (C++) methods of our Decoder class.
  if (javaEnvironment->RegisterNatives(
          decoderClass, nativeMethods, sizeof(nativeMethods) / sizeof(JNINativeMethod)) != 0)
    return "RegisterNatives failed.";
  return NULL;
}

static JNIEnv *getJavaEnvironment() {
  JNIEnv *javaEnvironment;
  return javaVM->AttachCurrentThread(&javaEnvironment, NULL) == JNI_OK ? javaEnvironment : NULL;
}

// Creates a Decoder instance in Java.
static const char *createJavaDecoder(decoderAndroidInternals *internals, JNIEnv **javaEnvironment) {
  *javaEnvironment = getJavaEnvironment();
  if (!(*javaEnvironment)) return "Can't attach current thread to Java.";
  jmethodID constructor = (*javaEnvironment)->GetMethodID(decoderClass, "<init>", "(J)V");
  if (!constructor) return "Can't get the Java Decoder constructor.";
  internals->javaDecoder = (*javaEnvironment)->NewObject(decoderClass, constructor, internals);
  if (!internals->javaDecoder) return "Java Decoder constructor failed.";
  internals->javaDecoder = (*javaEnvironment)->NewGlobalRef(internals->javaDecoder);
  if (!internals->javaDecoder) return "Can't make the Decoder instance global.";
  return NULL;
}

DecoderAndroidImplementation::DecoderAndroidImplementation(
    std::shared_ptr<DataProvider> dataProvider) {
  internals = new decoderAndroidInternals;
  memset(internals, 0, sizeof(decoderAndroidInternals));
  internals->dataProvider = dataProvider;
  internals->eof = false;
  internals->bufferCapacityFrames = 4096;
}

DecoderAndroidImplementation::~DecoderAndroidImplementation() {
  if (internals->javaDecoder) {
    JNIEnv *javaEnvironment = getJavaEnvironment();
    if (javaEnvironment) javaEnvironment->DeleteGlobalRef(internals->javaDecoder);
  }
  if (internals->floatBuffer) free(internals->floatBuffer);
  if (internals->intBuffer) free(internals->intBuffer);
  delete internals;
}

void DecoderAndroidImplementation::load(const ERROR_DECODER_CALLBACK &decoder_error_callback,
                                        const LOAD_DECODER_CALLBACK &decoder_load_callback) {
  // Create the Java Decoder.
  JNIEnv *javaEnvironment = NULL;
  const char *error = createJavaDecoder(internals, &javaEnvironment);
  if (error) {
    decoder_error_callback(error, 0);
    decoder_load_callback(false);
    return;
  }
  jstring errStr =
      (jstring)javaEnvironment->CallObjectMethod(internals->javaDecoder, errorMethodID, 0);
  if (errStr) {
    error = javaEnvironment->GetStringUTFChars(errStr, NULL);
    if (error) {
      decoder_error_callback(error, 0);
      decoder_load_callback(false);
      javaEnvironment->ReleaseStringUTFChars(errStr, error);
      return;
    }
  }

  // Get important metrics.
  internals->samplerate = javaEnvironment->GetIntField(internals->javaDecoder, samplerateFieldID);
  internals->channels = javaEnvironment->GetIntField(internals->javaDecoder, channelFieldID);
  internals->durationFrames =
      javaEnvironment->GetLongField(internals->javaDecoder, durationFieldID);

  // Allocate internal buffers.
  internals->floatBuffer =
      (float *)malloc(internals->channels * sizeof(float) * internals->bufferCapacityFrames);
  if (!internals->floatBuffer) {
    decoder_error_callback("Out of memory.", 0);
    decoder_load_callback(false);
    return;
  }
  internals->intBuffer = (short int *)malloc(JAVABUFFERSIZEBYTES);
  if (!internals->intBuffer) {
    decoder_error_callback("Out of memory.", 0);
    decoder_load_callback(false);
    return;
  }
  decoder_load_callback(true);
}

// Returns false on out of memory, true otherwise.
static bool putIntoFloatBuffer(decoderAndroidInternals *internals, int bytesRead) {
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
  short int *ints = internals->intBuffer;
  float *floats = internals->floatBuffer + internals->bufferWritePosFrames * internals->channels;
  int numSamples = framesRead * internals->channels;

  static const float multiplier = 1.0f / 32767.0f;  // Multiplying is much faster than dividing.
  while (numSamples--) *floats++ = float(*ints++) * multiplier;

  internals->bufferWritePosFrames += framesRead;
  internals->numFramesInBuffer += framesRead;
  return true;
}

void DecoderAndroidImplementation::decode(long frames,
                                          const DECODE_CALLBACK &decode_callback,
                                          bool synchronous) {
  // Creating the output buffer.
  float *outputBuffer = (float *)malloc(frames * internals->channels * sizeof(float));
  if (!outputBuffer) {
    decode_callback(internals->currentFrameIndex, 0, NULL);
    return;
  }

  JNIEnv *javaEnvironment = NULL;
  long framesRead = 0;
  while (framesRead < frames) {
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
    if (!javaEnvironment) {
      javaEnvironment = getJavaEnvironment();
      if (!javaEnvironment) break;
    }

    // Decode one frame with a Java representation of our int buffer.
    jobject javaBuffer =
        javaEnvironment->NewDirectByteBuffer(internals->intBuffer, JAVABUFFERSIZEBYTES);
    if (!javaBuffer) break;
    int bytesRead =
        javaEnvironment->CallIntMethod(internals->javaDecoder, decodeMethodID, javaBuffer);

    if (bytesRead == 0) {  // Error.
      jstring errStr =
          (jstring)javaEnvironment->CallObjectMethod(internals->javaDecoder, errorMethodID, 0);
      if (errStr) {
        const char *error = javaEnvironment->GetStringUTFChars(errStr, NULL);
        if (error) {
          javaEnvironment->ReleaseStringUTFChars(errStr, error);
          return;
        }
      }
      break;
    } else if (bytesRead == INT_MIN) {  // Error and eof.
      internals->eof = true;
      break;
    } else if (bytesRead < 0) {  // Read and eof.
      internals->eof = true;
      if (!putIntoFloatBuffer(internals, -bytesRead)) break;
    } else {  // Normal read.
      internals->eof = false;
      if (!putIntoFloatBuffer(internals, bytesRead)) break;
    }
  }

  // Passing the result.
  decode_callback(internals->currentFrameIndex, framesRead, outputBuffer);
  free(outputBuffer);
}

void DecoderAndroidImplementation::seek(long frame_index) {
  if (internals->currentFrameIndex == frame_index) return;
  JNIEnv *javaEnvironment = getJavaEnvironment();
  if (!javaEnvironment) return;
  if (javaEnvironment->CallBooleanMethod(internals->javaDecoder, seekMethodID, frame_index)) {
    internals->currentFrameIndex = (int)frame_index;
    internals->bufferWritePosFrames = internals->bufferReadPosFrames =
        internals->numFramesInBuffer = 0;
    internals->eof = false;
  }
}

void DecoderAndroidImplementation::flush() {
  JNIEnv *javaEnvironment = getJavaEnvironment();
  if (!javaEnvironment) return;
  javaEnvironment->CallBooleanMethod(
      internals->javaDecoder, seekMethodID, internals->currentFrameIndex);
  internals->bufferWritePosFrames = internals->bufferReadPosFrames = internals->numFramesInBuffer =
      0;
  internals->eof = false;
}

double DecoderAndroidImplementation::sampleRate() {
  return internals->samplerate;
}

int DecoderAndroidImplementation::channels() {
  return internals->channels;
}

const std::string &DecoderAndroidImplementation::path() {
  return internals->dataProvider->path();
}

const std::string &DecoderAndroidImplementation::name() {
  return domain;
}

long DecoderAndroidImplementation::currentFrameIndex() {
  return internals->currentFrameIndex;
}

long DecoderAndroidImplementation::frames() {
  return internals->durationFrames;
}

bool DecoderAndroidImplementation::eof() {
  return internals->eof;
}

}  // namespace decoder
}  // namespace nativeformat

#endif
