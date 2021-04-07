set(SOURCE_FILES
  ../include/NFDecoder/NFDecoderMimeTypes.h
  ../include/NFDecoder/Factory.h
  ../include/NFDecoder/Decoder.h
  ../include/NFDecoder/DataProvider.h
  ../include/NFDecoder/DataProviderFactory.h
  ../include/NFDecoder/Decrypter.h
  ../include/NFDecoder/DecrypterFactory.h
  ../include/NFDecoder/Manifest.h
  ../include/NFDecoder/ManifestFactory.h
  NFDecoderMimeTypes.cpp
  Factory.cpp
  Decoder.cpp
  FactoryCommonImplementation.h
  FactoryCommonImplementation.cpp
  FactoryNormalisationImplementation.h
  FactoryNormalisationImplementation.cpp
  FactoryAppleImplementation.h
  FactoryAppleImplementation.cpp
  FactoryLGPLImplementation.h
  FactoryLGPLImplementation.cpp
  DataProvider.cpp
  DataProviderFactory.cpp
  DataProviderFactoryImplementation.h
  DataProviderFactoryImplementation.cpp
  DataProviderFileImplementation.h
  DataProviderFileImplementation.cpp
  DecoderOggImplementation.h
  DecoderOggImplementation.cpp
  DecoderVorbisImplementation.h
  DecoderVorbisImplementation.cpp
  DecoderOpusImplementation.h
  DecoderOpusImplementation.cpp
  DataProviderHTTPImplementation.h
  DataProviderHTTPImplementation.cpp
  DecoderWavImplementation.h
  DecoderWavImplementation.cpp
  DecoderAudioConverterImplementation.h
  DecoderAudioConverterImplementation.cpp
  DecoderAVCodecImplementation.h
  DecoderAVCodecImplementation.cpp
  DecoderNormalisationImplementation.h
  DecoderNormalisationImplementation.cpp
  DecoderMidiImplementation.h
  DecoderMidiImplementation.cpp
  base64.h
  base64.cpp
  DecrypterFactory.cpp
  DecrypterFactoryImplementation.h
  DecrypterFactoryImplementation.cpp
  Path.h
  Path.cpp
  LicenseManager.h
  ManifestFactory.cpp
  ManifestFactoryImplementation.h
  ManifestFactoryImplementation.cpp
  Decrypter.cpp
  LicenseManager.cpp
  FactoryServiceImplementation.h
  FactoryServiceImplementation.cpp
  DataProviderMemoryImplementation.h
  DataProviderMemoryImplementation.cpp
  DecoderFLACImplementation.h
  DecoderFLACImplementation.cpp
  DecoderDashToHLSTransmuxerImplementation.h
  DecoderDashToHLSTransmuxerImplementation.cpp
  FactoryTransmuxerImplementation.h
  FactoryTransmuxerImplementation.cpp
  DecoderAndroidImplementation.h
  DecoderAndroidImplementation.cpp
  FactoryAndroidImplementation.h
  FactoryAndroidImplementation.cpp
  DecoderSpeexImplementation.h
  DecoderSpeexImplementation.cpp)
set(LINK_LIBRARIES
  ogg
  vorbis
  vorbisfile
  opus
  opusfile
  libresample
  NFHTTP
  ${OPENSSL_LIBRARIES}
  flac
  TinySoundFont
  speex
  )
set(INCLUDE_DIRS
  ../include
  ../libraries/vorbis/include
  ../libraries/opus/include
  ${OPUSFILE_INCLUDE_DIR}
  ../libraries/ogg/include
  ../libraries/NFHTTP/include
  ../libraries/universal-dash-transmuxer/include
  ../libraries/speex/include
  ${CMAKE_BINARY_DIR}/output)

if(INCLUDE_UDT)
  set(DASH_LIBRARIES
    DashToHlsLibrary
    DashToHlsDash
    DashToHlsTs
    DashToHlsPs
  )
  list(APPEND LINK_LIBRARIES ${DASH_LIBRARIES})
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  find_library(AUDIO_TOOLBOX AudioToolbox)
  list(APPEND LINK_LIBRARIES ${AUDIO_TOOLBOX})
endif()

if(FFMPEG_FOUND)
  list(APPEND LINK_LIBRARIES
    AvCodec
    AvDevice
    AvFilter
    AvFormat
    AvResample
    AvUtil
    SwResample
    SwScale)
  list(APPEND INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR})
endif()

add_library(NFDecoder STATIC ${SOURCE_FILES})
target_include_directories(NFDecoder PUBLIC ../include PRIVATE ${INCLUDE_DIRS})
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  target_link_libraries(NFDecoder ${LINK_LIBRARIES})
else()
  target_link_libraries(NFDecoder -Wl,--start-group
                                  ${LINK_LIBRARIES}
                                  -Wl,--end-group)
endif()

if(NOT IOS)
  add_subdirectory(cli)
endif()

if(USE_FFMPEG)
  target_compile_definitions(NFDecoder PRIVATE USE_FFMPEG=1)
endif()

if(INCLUDE_UDT)
  add_definitions(-DINCLUDE_UDT=1)
endif()
