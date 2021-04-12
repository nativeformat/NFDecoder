include(CheckIncludeFiles)

set(BUILD_FRAMEWORK OFF)
set(BUILD_SHARED_LIBS OFF)

add_subdirectory(ogg)

## VORBIS-SPECIFIC BEGIN
##
# Vorbis's cmakelists expects libogg artifacts to exist at configure time.
# The following copy/paste avoids this.

# Extract project version from configure.ac
file(READ vorbis/configure.ac CONFIGURE_AC_CONTENTS)
string(REGEX MATCH "AC_INIT\\(\\[libvorbis\\],\\[([0-9]*).([0-9]*).([0-9]*)" DUMMY ${CONFIGURE_AC_CONTENTS})
set(VORBIS_PROJECT_VERSION_MAJOR ${CMAKE_MATCH_1})
set(VORBIS_PROJECT_VERSION_MINOR ${CMAKE_MATCH_2})
set(VORBIS_PROJECT_VERSION_PATCH ${CMAKE_MATCH_3})
set(VORBIS_PROJECT_VERSION ${VORBIS_PROJECT_VERSION_MAJOR}.${VORBIS_PROJECT_VERSION_MINOR}.${VORBIS_PROJECT_VERSION_PATCH})

# Helper function to get version-info
function(get_version_info result current_var_name age_var_name revision_var_name)
  string(REGEX MATCH "${current_var_name}=([0-9]*)" DUMMY ${CONFIGURE_AC_CONTENTS})
  set(VORBIS_VERSION_INFO_CURRENT ${CMAKE_MATCH_1})

  string(REGEX MATCH "${age_var_name}=([0-9]*)" DUMMY ${CONFIGURE_AC_CONTENTS})
  set(VORBIS_VERSION_INFO_AGE ${CMAKE_MATCH_1})

  string(REGEX MATCH "${revision_var_name}=([0-9]*)" DUMMY ${CONFIGURE_AC_CONTENTS})
  set(VORBIS_VERSION_INFO_REVISION ${CMAKE_MATCH_1})

  math(EXPR VORBIS_VERSION_INFO_CURRENT_MINUS_AGE "${VORBIS_VERSION_INFO_CURRENT} - ${VORBIS_VERSION_INFO_AGE}")

  set(${result} "${VORBIS_VERSION_INFO_CURRENT_MINUS_AGE}.${VORBIS_VERSION_INFO_AGE}.${VORBIS_VERSION_INFO_REVISION}" PARENT_SCOPE)
endfunction()

# OGG_LIBRARIES is used in target_link_libraries within the following directory,
# so we're actually linking to a target name.
set(OGG_LIBRARIES ogg)
set(OGG_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/ogg/include")
# Add custom code changes to opusfile
execute_process(COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/vorbis.patch
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vorbis)
add_subdirectory(vorbis/lib)
##
## VORBIS-SPECIFIC END

## OPUS
set(SILK_SOURCES
silk/CNG.c
silk/code_signs.c
silk/init_decoder.c
silk/decode_core.c
silk/decode_frame.c
silk/decode_parameters.c
silk/decode_indices.c
silk/decode_pulses.c
silk/decoder_set_fs.c
silk/dec_API.c
silk/enc_API.c
silk/encode_indices.c
silk/encode_pulses.c
silk/gain_quant.c
silk/interpolate.c
silk/LP_variable_cutoff.c
silk/NLSF_decode.c
silk/NSQ.c
silk/NSQ_del_dec.c
silk/PLC.c
silk/shell_coder.c
silk/tables_gain.c
silk/tables_LTP.c
silk/tables_NLSF_CB_NB_MB.c
silk/tables_NLSF_CB_WB.c
silk/tables_other.c
silk/tables_pitch_lag.c
silk/tables_pulses_per_block.c
silk/VAD.c
silk/control_audio_bandwidth.c
silk/quant_LTP_gains.c
silk/VQ_WMat_EC.c
silk/HP_variable_cutoff.c
silk/NLSF_encode.c
silk/NLSF_VQ.c
silk/NLSF_unpack.c
silk/NLSF_del_dec_quant.c
silk/process_NLSFs.c
silk/stereo_LR_to_MS.c
silk/stereo_MS_to_LR.c
silk/check_control_input.c
silk/control_SNR.c
silk/init_encoder.c
silk/control_codec.c
silk/A2NLSF.c
silk/ana_filt_bank_1.c
silk/biquad_alt.c
silk/bwexpander_32.c
silk/bwexpander.c
silk/debug.c
silk/decode_pitch.c
silk/inner_prod_aligned.c
silk/lin2log.c
silk/log2lin.c
silk/LPC_analysis_filter.c
silk/LPC_inv_pred_gain.c
silk/table_LSF_cos.c
silk/NLSF2A.c
silk/NLSF_stabilize.c
silk/NLSF_VQ_weights_laroia.c
silk/pitch_est_tables.c
silk/resampler.c
silk/resampler_down2_3.c
silk/resampler_down2.c
silk/resampler_private_AR2.c
silk/resampler_private_down_FIR.c
silk/resampler_private_IIR_FIR.c
silk/resampler_private_up2_HQ.c
silk/resampler_rom.c
silk/sigm_Q15.c
silk/sort.c
silk/sum_sqr_shift.c
silk/stereo_decode_pred.c
silk/stereo_encode_pred.c
silk/stereo_find_predictor.c
silk/stereo_quant_pred.c
silk/LPC_fit.c
)

set(SILK_SOURCES_SSE4_1
silk/x86/NSQ_sse.c
silk/x86/NSQ_del_dec_sse.c
silk/x86/x86_silk_map.c
silk/x86/VAD_sse.c
silk/x86/VQ_WMat_EC_sse.c
)

set(SILK_SOURCES_ARM_NEON_INTR
silk/arm/arm_silk_map.c
silk/arm/NSQ_del_dec_neon_intr.c
silk/arm/NSQ_neon.c
)

set(SILK_SOURCES_FIXED
silk/fixed/LTP_analysis_filter_FIX.c
silk/fixed/LTP_scale_ctrl_FIX.c
silk/fixed/corrMatrix_FIX.c
silk/fixed/encode_frame_FIX.c
silk/fixed/find_LPC_FIX.c
silk/fixed/find_LTP_FIX.c
silk/fixed/find_pitch_lags_FIX.c
silk/fixed/find_pred_coefs_FIX.c
silk/fixed/noise_shape_analysis_FIX.c
silk/fixed/process_gains_FIX.c
silk/fixed/regularize_correlations_FIX.c
silk/fixed/residual_energy16_FIX.c
silk/fixed/residual_energy_FIX.c
silk/fixed/warped_autocorrelation_FIX.c
silk/fixed/apply_sine_window_FIX.c
silk/fixed/autocorr_FIX.c
silk/fixed/burg_modified_FIX.c
silk/fixed/k2a_FIX.c
silk/fixed/k2a_Q16_FIX.c
silk/fixed/pitch_analysis_core_FIX.c
silk/fixed/vector_ops_FIX.c
silk/fixed/schur64_FIX.c
silk/fixed/schur_FIX.c
)

set(SILK_SOURCES_FIXED_SSE4_1
silk/fixed/x86/vector_ops_FIX_sse.c
silk/fixed/x86/burg_modified_FIX_sse.c
)

set(SILK_SOURCES_FLOAT
silk/float/apply_sine_window_FLP.c
silk/float/corrMatrix_FLP.c
silk/float/encode_frame_FLP.c
silk/float/find_LPC_FLP.c
silk/float/find_LTP_FLP.c
silk/float/find_pitch_lags_FLP.c
silk/float/find_pred_coefs_FLP.c
silk/float/LPC_analysis_filter_FLP.c
silk/float/LTP_analysis_filter_FLP.c
silk/float/LTP_scale_ctrl_FLP.c
silk/float/noise_shape_analysis_FLP.c
silk/float/process_gains_FLP.c
silk/float/regularize_correlations_FLP.c
silk/float/residual_energy_FLP.c
silk/float/warped_autocorrelation_FLP.c
silk/float/wrappers_FLP.c
silk/float/autocorrelation_FLP.c
silk/float/burg_modified_FLP.c
silk/float/bwexpander_FLP.c
silk/float/energy_FLP.c
silk/float/inner_product_FLP.c
silk/float/k2a_FLP.c
silk/float/LPC_inv_pred_gain_FLP.c
silk/float/pitch_analysis_core_FLP.c
silk/float/scale_copy_vector_FLP.c
silk/float/scale_vector_FLP.c
silk/float/schur_FLP.c
silk/float/sort_FLP.c
)

set(CELT_SOURCES
celt/bands.c
celt/celt.c
celt/celt_encoder.c
celt/celt_decoder.c
celt/cwrs.c
celt/entcode.c
celt/entdec.c
celt/entenc.c
celt/kiss_fft.c
celt/laplace.c
celt/mathops.c
celt/mdct.c
celt/modes.c
celt/pitch.c
celt/celt_lpc.c
celt/quant_bands.c
celt/rate.c
celt/vq.c
)

set(CELT_SOURCES_SSE
celt/x86/x86cpu.c
celt/x86/x86_celt_map.c
celt/x86/pitch_sse.c
)

set(CELT_SOURCES_SSE2
celt/x86/pitch_sse2.c celt/x86/vq_sse2.c
)

set(CELT_SOURCES_SSE4_1
celt/x86/celt_lpc_sse.c
celt/x86/pitch_sse4_1.c
)

set(CELT_SOURCES_ARM
celt/arm/armcpu.c
celt/arm/arm_celt_map.c
)

set(CELT_SOURCES_ARM_ASM
celt/arm/celt_pitch_xcorr_arm.s
)

set(CELT_AM_SOURCES_ARM_ASM
celt/arm/armopts.s.in
)

set(CELT_SOURCES_ARM_NEON_INTR
celt/arm/celt_neon_intr.c
)

set(CELT_SOURCES_ARM_NE10
celt/arm/celt_ne10_fft.c
celt/arm/celt_ne10_mdct.c
)

set(OPUS_SOURCES
src/opus.c
src/opus_decoder.c
src/opus_encoder.c
src/opus_multistream.c
src/opus_multistream_encoder.c
src/opus_multistream_decoder.c
src/repacketizer.c
)

set(OPUS_SOURCES_FLOAT
src/analysis.c
src/mlp.c
src/mlp_data.c
)

if(FIXED_POINT)
    list(APPEND SILK_SOURCES ${SILK_SOURCES_FIXED})
    if(HAVE_SSE4_1)
        list(APPEND SILK_SOURCES ${SILK_SOURCES_SSE4_1} ${SILK_SOURCES_FIXED_SSE4_1})
    endif(HAVE_SSE4_1)
else(FIXED_POINT)
    list(APPEND SILK_SOURCES ${SILK_SOURCES_FLOAT})
    if (HAVE_SSE4_1)
        list(APPEND SILK_SOURCES ${SILK_SOURCES_SSE4_1})
    endif(HAVE_SSE4_1)
endif(FIXED_POINT)

if (DISABLE_FLOAT_API)
else(DISABLE_FLOAT_API)
    list(APPEND OPUS_SOURCES ${OPUS_SOURCES_FLOAT})
endif(DISABLE_FLOAT_API)

if(HAVE_SSE)
    list(APPEND CELT_SOURCES ${CELT_SOURCES_SSE})
endif(HAVE_SSE)

if(HAVE_SSE2)
    list(APPEND CELT_SOURCES ${CELT_SOURCES_SSE2})
endif(HAVE_SSE2)

if(HAVE_SSE4_1)
    list(APPEND CELT_SOURCES ${CELT_SOURCES_SSE4_1})
endif(HAVE_SSE4_1)

if(CPU_ARM)
    list(APPEND CELT_SOURCES ${CELT_SOURCES_ARM})
    list(APPEND SILK_SOURCES ${SILK_SOURCES_ARM})

    if(HAVE_ARM_NEON_INTR)
        list(APPEND CELT_SOURCES ${CELT_SOURCES_ARM_NEON_INTR})
        list(APPEND SILK_SOURCES ${SILK_SOURCES_ARM_NEON_INTR})
    endif(HAVE_ARM_NEON_INTR)

    if(HAVE_ARM_NE10)
        list(APPEND CELT_SOURCES ${CELT_SOURCES_ARM_NE10})
    endif(HAVE_ARM_NE10)

    #if(OPUS_ARM_EXTERNAL_ASM)
    #    noinst_LTLIBRARIES = libarmasm.la
    #    libarmasm_la_SOURCES = $(CELT_SOURCES_ARM_ASM:.s=-gnu.S)
    #    BUILT_SOURCES = $(CELT_SOURCES_ARM_ASM:.s=-gnu.S) \
    #    $(CELT_AM_SOURCES_ARM_ASM:.s.in=.s) \
    #    $(CELT_AM_SOURCES_ARM_ASM:.s.in=-gnu.S)
    #endif(OPUS_ARM_EXTERNAL_ASM)
endif(CPU_ARM)

#add_definitions(-DOPUS_VERSION="\\"1.0.1\\"")

# It is strongly recommended to uncomment one of these
# VAR_ARRAYS: Use C99 variable-length arrays for stack allocation
# USE_ALLOCA: Use alloca() for stack allocation
# If none is defined, then the fallback is a non-threadsafe global array
if (USE_ALLOCA)
    add_definitions(-DUSE_ALLOCA)
else (USE_ALLOCA)
    add_definitions(-DVAR_ARRAYS)
endif (USE_ALLOCA)

# These options affect performance
# HAVE_LRINTF: Use C99 intrinsics to speed up float-to-int conversion
add_definitions(-DHAVE_LRINTF)

if(UNIX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -W -Wstrict-prototypes -Wextra -Wcast-align")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wnested-externs -Wshadow")
endif(UNIX)

add_definitions(-DOPUS_BUILD)

if (FIXED_POINT)
    add_definitions(-DFIXED_POINT=1 -DDISABLE_FLOAT_API)
endif (FIXED_POINT)

include_directories(include opus/silk opus/silk/float opus/silk/fixed opus/celt opus/src opus/include)

# add opus prefix
set(ALL_OPUS_SOURCES)
foreach(src_file ${CELT_SOURCES} ${SILK_SOURCES} ${OPUS_SOURCES})
  list(APPEND ALL_OPUS_SOURCES opus/${src_file})
endforeach(src_file)

add_library(opus STATIC ${ALL_OPUS_SOURCES})
target_compile_options(opus PRIVATE "-Wno-null-pointer-arithmetic")

## OPUSFILE
set(OPUSFILE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/opusfile/include CACHE STRING "opusfile includes" FORCE)
set(OPUSFILE_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/opusfile/src/http.c
	${CMAKE_CURRENT_SOURCE_DIR}/opusfile/src/info.c
	${CMAKE_CURRENT_SOURCE_DIR}/opusfile/src/internal.c
	${CMAKE_CURRENT_SOURCE_DIR}/opusfile/src/internal.h
	${CMAKE_CURRENT_SOURCE_DIR}/opusfile/src/opusfile.c
	${CMAKE_CURRENT_SOURCE_DIR}/opusfile/src/stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/opusfile/src/wincerts.c
	${CMAKE_CURRENT_SOURCE_DIR}/opusfile/src/winerrno.h
)

set(OPUSFILE_INCLUDE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/opusfile/include/opusfile.h
)

# Add custom code changes to opusfile
execute_process(COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/opusfile.patch
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/opusfile)

add_library(opusfile STATIC ${OPUSFILE_SOURCE_FILES} ${OPUSFILE_INCLUDE_FILES})
target_include_directories(opusfile PUBLIC ${OPUSFILE_INCLUDE_DIR} ${OGG_INCLUDE_DIRS})

## FLAC
set(FLAC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/flac/include CACHE STRING "FLAC includes" FORCE)
set(FLAC_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/bitmath.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/bitreader.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/bitwriter.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/cpu.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/crc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/fixed_intrin_sse2.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/fixed_intrin_ssse3.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/fixed.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/float.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/format.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/lpc_intrin_avx2.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/lpc_intrin_sse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/lpc_intrin_sse2.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/lpc_intrin_sse41.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/lpc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/md5.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/memory.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/metadata_iterators.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/metadata_object.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/ogg_decoder_aspect.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/ogg_encoder_aspect.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/ogg_helper.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/ogg_mapping.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/stream_decoder.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/stream_encoder_framing.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/stream_encoder_intrin_avx2.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/stream_encoder_intrin_sse2.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/stream_encoder_intrin_ssse3.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/stream_encoder.c
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/window.c
#  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/windows_unicode_filenames.c
)
set(FLAC_INCLUDE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/include/FLAC/all.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/include/FLAC/assert.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/include/FLAC/callback.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/include/FLAC/export.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/include/FLAC/format.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/include/FLAC/metadata.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/include/FLAC/ordinals.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/include/FLAC/stream_decoder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/include/FLAC/stream_encoder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/all.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/bitmath.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/bitreader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/bitwriter.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/cpu.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/crc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/fixed.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/float.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/format.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/lpc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/macros.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/md5.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/memory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/metadata.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/ogg_decoder_aspect.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/ogg_encoder_aspect.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/ogg_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/ogg_mapping.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/stream_encoder_framing.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/stream_encoder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/private/window.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/protected/all.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/protected/stream_decoder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include/protected/stream_encoder.h
)
add_library(flac STATIC ${FLAC_SOURCE_FILES} ${FLAC_INCLUDE_FILES})
target_include_directories(flac PUBLIC ${FLAC_INCLUDE_DIR} ${OGG_INCLUDE_DIRS} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/flac/src/libFLAC/include)
target_compile_definitions(flac PRIVATE PACKAGE_VERSION="1.2" HAVE_LROUND=1 FLAC__HAS_OGG=1 HAVE_STDINT_H=1)

set(libresample_INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libresample/include)
set(libresample_CONFIG_FILE ${CMAKE_CURRENT_BINARY_DIR}/decoder/config.h)
check_include_files(inttypes.h HAVE_INTTYPES_H)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${libresample_CONFIG_FILE})
add_library(libresample STATIC libresample/src/configtemplate.h
                               libresample/src/filterkit.c
                               libresample/src/filterkit.h
                               libresample/src/resample_defs.h
                               libresample/src/resample.c
                               libresample/src/resamplesubs.c
                               ${libresample_CONFIG_FILE})
target_include_directories(libresample PUBLIC ${libresample_INCLUDE_DIRECTORY} ${CMAKE_CURRENT_BINARY_DIR}/decoder)
target_compile_options(libresample PRIVATE "-Wno-sign-compare")

if(INCLUDE_LGPL)
  # TODO: Insert retrieval of FFMPEG here
  set(FFMPEG_LIBRARY_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  set(FFMPEG_LIBRARY_TYPE "SHARED")
  if(UNIX AND NOT APPLE)
      set(FFMPEG_LIBRARY_DIR "/usr/lib/x86_64-linux-gnu/")
      set(FFMPEG_INCLUDE_DIR "/usr/include/x86_64-linux-gnu/")
      # set(FFMPEG_LIBRARY_SUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})
      # set(FFMPEG_LIBRARY_TYPE "STATIC")
  endif()

  set(FFMPEG_FOUND ON)
  set(LIBAVCODEC_PATH "${FFMPEG_LIBRARY_DIR}/libavcodec${FFMPEG_LIBRARY_SUFFIX}")
  set(LIBAVDEVICE_PATH "${FFMPEG_LIBRARY_DIR}/libavdevice${FFMPEG_LIBRARY_SUFFIX}")
  set(LIBAVFILTER_PATH "${FFMPEG_LIBRARY_DIR}/libavfilter${FFMPEG_LIBRARY_SUFFIX}")
  set(LIBAVFORMAT_PATH "${FFMPEG_LIBRARY_DIR}/libavformat${FFMPEG_LIBRARY_SUFFIX}")
  set(LIBAVRESAMPLE_PATH "${FFMPEG_LIBRARY_DIR}/libavresample${FFMPEG_LIBRARY_SUFFIX}")
  set(LIBAVUTIL_PATH "${FFMPEG_LIBRARY_DIR}/libavutil${FFMPEG_LIBRARY_SUFFIX}")
  set(LIBSWRESAMPLE_PATH "${FFMPEG_LIBRARY_DIR}/libswresample${FFMPEG_LIBRARY_SUFFIX}")
  set(LIBSWSCALE_PATH "${FFMPEG_LIBRARY_DIR}/libswscale${FFMPEG_LIBRARY_SUFFIX}")
  set(FFMPEG_FOUND ${FFMPEG_FOUND} PARENT_SCOPE)
  # This is a hack to make libavcodec work on Unix systems
  if(UNIX)
    foreach(INCLUDE_DIR ${FFMPEG_INCLUDE_DIR})
      string(LENGTH ${INCLUDE_DIR} INCLUDE_DIR_LENGTH)
      math(EXPR INCLUDE_DIR_INDEX "${INCLUDE_DIR_LENGTH}-9")
      string(SUBSTRING ${INCLUDE_DIR} ${INCLUDE_DIR_INDEX} 9 INCLUDE_DIR_LAST_PATH)
      if(${INCLUDE_DIR_LAST_PATH} MATCHES "libavutil")
        message("Removing libavutil include path")
        list(REMOVE_ITEM FFMPEG_INCLUDE_DIR ${INCLUDE_DIR})
      endif()
    endforeach()

    # Resolve symlinks to the real file path
    GET_FILENAME_COMPONENT(LIBAVCODEC_PATH ${LIBAVCODEC_PATH} REALPATH)
    GET_FILENAME_COMPONENT(LIBAVDEVICE_PATH ${LIBAVDEVICE_PATH} REALPATH)
    GET_FILENAME_COMPONENT(LIBAVFILTER_PATH ${LIBAVFILTER_PATH} REALPATH)
    GET_FILENAME_COMPONENT(LIBAVFORMAT_PATH ${LIBAVFORMAT_PATH} REALPATH)
    GET_FILENAME_COMPONENT(LIBAVRESAMPLE_PATH ${LIBAVRESAMPLE_PATH} REALPATH)
    GET_FILENAME_COMPONENT(LIBAVUTIL_PATH ${LIBAVUTIL_PATH} REALPATH)
    GET_FILENAME_COMPONENT(LIBSWRESAMPLE_PATH ${LIBSWRESAMPLE_PATH} REALPATH)
    GET_FILENAME_COMPONENT(LIBSWSCALE_PATH ${LIBSWSCALE_PATH} REALPATH)

    # Add install instructions for debian packaging
    install(FILES
      "${LIBAVCODEC_PATH}"
      "${LIBAVDEVICE_PATH}"
      "${LIBAVFILTER_PATH}"
      "${LIBAVFORMAT_PATH}"
      "${LIBAVRESAMPLE_PATH}"
      "${LIBAVUTIL_PATH}"
      "${LIBSWRESAMPLE_PATH}"
      "${LIBSWSCALE_PATH}"
      DESTINATION /usr/lib)
  endif()
  set(FFMPEG_INCLUDE_DIR ${FFMPEG_INCLUDE_DIR} PARENT_SCOPE)

  add_library(AvCodec ${FFMPEG_LIBRARY_TYPE} IMPORTED GLOBAL)
  add_library(AvDevice ${FFMPEG_LIBRARY_TYPE} IMPORTED GLOBAL)
  add_library(AvFilter ${FFMPEG_LIBRARY_TYPE} IMPORTED GLOBAL)
  add_library(AvFormat ${FFMPEG_LIBRARY_TYPE} IMPORTED GLOBAL)
  add_library(AvResample ${FFMPEG_LIBRARY_TYPE} IMPORTED GLOBAL)
  add_library(AvUtil ${FFMPEG_LIBRARY_TYPE} IMPORTED GLOBAL)
  add_library(SwResample ${FFMPEG_LIBRARY_TYPE} IMPORTED GLOBAL)
  add_library(SwScale ${FFMPEG_LIBRARY_TYPE} IMPORTED GLOBAL)
  set_target_properties(AvCodec PROPERTIES IMPORTED_LOCATION ${LIBAVCODEC_PATH})
  set_target_properties(AvDevice PROPERTIES IMPORTED_LOCATION ${LIBAVDEVICE_PATH})
  set_target_properties(AvFilter PROPERTIES IMPORTED_LOCATION ${LIBAVFILTER_PATH})
  set_target_properties(AvFormat PROPERTIES IMPORTED_LOCATION ${LIBAVFORMAT_PATH})
  set_target_properties(AvResample PROPERTIES IMPORTED_LOCATION ${LIBAVRESAMPLE_PATH})
  set_target_properties(AvUtil PROPERTIES IMPORTED_LOCATION ${LIBAVUTIL_PATH})
  set_target_properties(SwResample PROPERTIES IMPORTED_LOCATION ${LIBSWRESAMPLE_PATH})
  set_target_properties(SwScale PROPERTIES IMPORTED_LOCATION ${LIBSWSCALE_PATH})
endif()

add_subdirectory(NFHTTP)

# Generate Cmake from GYP
set(GYP_COMMAND gyp)
if(ANDROID_WINDOWS)
  set(GYP_COMMAND "${CMAKE_SOURCE_DIR}/nfdecoder_env/Scripts/gyp.exe")
endif()

# Generate Cmake from GYP in UDT
if(NOT DEFINED INCLUDE_UDT)
  set(INCLUDE_UDT ON)
endif()
if(INCLUDE_UDT)
  # Add custom code changes to UDT
  execute_process(COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/udt.patch
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/universal-dash-transmuxer)
  message(STATUS "generating cmake from gyp in UDT")
  execute_process(COMMAND ${GYP_COMMAND} -f cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/universal-dash-transmuxer/library/DashToHls.gyp
    --depth ${CMAKE_CURRENT_SOURCE_DIR}/universal-dash-transmuxer/library
    WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE retcode)
  if(NOT "${retcode}" STREQUAL "0")
    message(FATAL_ERROR "Could not generate CMake via GYP for UDT")
  endif()
  execute_process(COMMAND python ../tools/fix-udt-cmake.py
    "${CMAKE_CURRENT_SOURCE_DIR}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

set(BUILD_SHARED_LIBS OFF)
set(BUILD_SAMPLES OFF)
set(BUILD_TESTS OFF)
set(SHARED_PTR_INCLUDE "\#include <memory>")
set(SHARED_PTR_DEFINE "using std::shared_ptr")
if(INCLUDE_UDT)
  add_subdirectory(universal-dash-transmuxer/library/out/Default)
endif()

### TinySoundFont a convenient MIDI lib
set(TINYSOUNDFONT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/TinySoundFont)
add_library(TinySoundFont INTERFACE)
target_include_directories(TinySoundFont INTERFACE ${TINYSOUNDFONT_INCLUDE_DIR})

# speex
set(SPEEX_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/speex/include CACHE STRING "SPEEX includes" FORCE)
set(SPEEXDSP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/include CACHE STRING "SPEEX includes" FORCE)
set(SPEEX_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/bits.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/cb_search.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/exc_5_64_table.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/exc_5_256_table.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/exc_8_128_table.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/exc_10_16_table.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/exc_10_32_table.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/exc_20_32_table.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/fftwrap.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/filters.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/gain_table_lbr.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/gain_table.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/hexc_10_32_table.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/hexc_table.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/high_lsp_tables.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/kiss_fft.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/kiss_fftr.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/lpc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/lsp_tables_nb.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/lsp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/ltp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/modes_wb.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/modes.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/nb_celp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/quant_lsp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/sb_celp.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/smallft.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/speex_callbacks.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/speex.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/stereo.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/vbr.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/vorbis_psy.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/vq.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/window.c)
set(SPEEXDSP_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/buffer.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/fftwrap.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/filterbank.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/jitter.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/kiss_fft.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/kiss_fftr.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/mdf.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/preprocess.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/resample.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/scal.c
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/smallft.c)
set(SPEEX_INCLUDE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/include/speex/speex_bits.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/include/speex/speex_callbacks.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/include/speex/speex_header.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/include/speex/speex_stereo.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/include/speex/speex_types.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/include/speex/speex.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/_kiss_fft_guts.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/arch.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/cb_search_arm4.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/cb_search_bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/cb_search_sse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/cb_search.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/fftwrap.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/filters_arm4.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/filters_bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/filters_sse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/filters.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/fixed_arm4.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/fixed_arm5e.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/fixed_bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/fixed_debug.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/fixed_generic.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/kiss_fft.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/kiss_fftr.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/lpc_bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/lpc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/lsp_bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/lsp.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/ltp_arm4.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/ltp_bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/ltp_sse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/ltp.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/math_approx.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/misc_bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/modes.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/nb_celp.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/os_support.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/quant_lsp_bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/sb_celp.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/smallft.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/stack_alloc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/vbr.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/vorbis_psy.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/vq_arm4.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/vq_bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/vq_sse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex/vq.h)
set(SPEEXDSP_INCLUDE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/include/speex/speex_buffer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/include/speex/speex_echo.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/include/speex/speex_jitter.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/include/speex/speex_preprocess.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/include/speex/speex_resampler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/include/speex/speexdsp_types.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/_kiss_fft_guts.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/arch.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/fftwrap.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/filterbank.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/fixed_arm4.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/fixed_arm5e.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/fixed_bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/fixed_debug.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/fixed_generic.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/kiss_fft.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/kiss_fftr.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/math_approx.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/misc_bfin.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/os_support.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/pseudofloat.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/resample_neon.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/resample_sse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/smallft.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/stack_alloc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp/vorbis_psy.h)
add_library(speex STATIC ${SPEEX_SOURCE_FILES} ${SPEEX_INCLUDE_FILES})
add_library(speexdsp STATIC ${SPEEXDSP_SOURCE_FILES} ${SPEEXDSP_INCLUDE_FILES})
set(speex_CONFIG_FILE ${CMAKE_CURRENT_BINARY_DIR}/include/speex/speex_config_types.h)
set(speexdsp_CONFIG_FILE ${CMAKE_CURRENT_BINARY_DIR}/include/speex/speexdsp_config_types.h)
set(SIZE16 short)
set(USIZE16 unsigned\ short)
set(SIZE32 int)
set(USIZE32 unsigned\ int)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/speex/include/speex/speex_config_types.h.in ${speex_CONFIG_FILE})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/include/speex/speexdsp_config_types.h.in ${speexdsp_CONFIG_FILE})
target_include_directories(speex PUBLIC ${SPEEX_INCLUDE_DIR} ${SPEEX_INCLUDE_DIR}/speex/ ${CMAKE_CURRENT_BINARY_DIR}/include/speex/ PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/speex/libspeex)
target_include_directories(speexdsp PUBLIC ${SPEEXDSP_INCLUDE_DIR} ${SPEEXDSP_INCLUDE_DIR}/speex/ ${CMAKE_CURRENT_BINARY_DIR}/include/speex/ PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/speexdsp/libspeexdsp)
target_compile_definitions(speex PRIVATE FIXED_POINT=1 EXPORT=__attribute__\(\(visibility\(\"default\"\)\)\) USE_SMALLFT=1)
target_compile_definitions(speexdsp PRIVATE FIXED_POINT=1 EXPORT=__attribute__\(\(visibility\(\"default\"\)\)\) USE_SMALLFT=1)
target_compile_options(speex PRIVATE
  "-Wno-unused-parameter"
  "-Wno-shadow"
  "-Wno-cast-align"
  "-Wno-incompatible-pointer-types"
  "-Wno-unused-function")
target_compile_options(speexdsp PRIVATE
  "-Wno-unused-variable"
  "-Wno-cast-align"
  "-Wno-unused-parameter"
  "-Wno-sign-compare"
  "-Wno-incompatible-pointer-types"
  "-Wno-incompatible-pointer-types"
  "-Wno-shadow"
  "-Wno-unused-function")
