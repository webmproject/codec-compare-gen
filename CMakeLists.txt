# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.20)
project(
  codec-compare-gen
  LANGUAGES CXX
  VERSION 0.7.9)
set(CMAKE_CXX_STANDARD 17)

# Options

option(CCGEN_ENABLE_AVIF "Build with AVIF support (libavif[aom+dav1d])" ON)
option(CCGEN_ENABLE_AVIF_LIBHEIF "Build with AVIF support (libheif[aom+dav1d])"
       OFF)
option(CCGEN_ENABLE_JPEG "Build with JPEG support (Turbo, Moz, sjpeg)" ON)
option(CCGEN_ENABLE_JPEGLI "Build with Jpegli support (jpegli)" ON)
option(CCGEN_ENABLE_JPEGZUNE
       "Build with Rust JPEG support (image and zune-jpeg crates)" OFF)
option(CCGEN_ENABLE_JPEG2000 "Build with JPEG 2000 support (OpenJPEG)" OFF)
option(CCGEN_ENABLE_JPEGXL "Build with JPEG XL support (libjxl)" ON)
option(CCGEN_ENABLE_WEBP "Build with WebP support (libwebp)" ON)
option(CCGEN_ENABLE_WEBPRS
       "Build with Rust WebP support (image and image-webp crates)" OFF)
option(CCGEN_ENABLE_WEBP2 "Build with WebP 2 support (libwebp2) [REQUIRED]" ON)
option(CCGEN_ENABLE_FFV1 "Build with FFV1 support (libavcodec from FFmpeg)" OFF)
option(CCGEN_ENABLE_BASIS "Build with Basis Universal support" OFF)

option(CCGEN_ENABLE_DSSIM "Build the DSSIM distortion metric binary" ON)

option(CCGEN_WASM "Build the WASM codec-compare-gen library with emscripten"
       OFF)

option(BUILD_SHARED_LIBS "Build the shared codec-compare-gen library" ON)
if(BUILD_SHARED_LIBS)
  set(CCGEN_PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}")
  set(CCGEN_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
else()
  set(CCGEN_PREFIX "${CMAKE_STATIC_LIBRARY_PREFIX}")
  set(CCGEN_SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

option(CCGEN_BUILD_TESTING "Build the tests (requires GoogleTest)" OFF)
if(NOT CCGEN_BUILD_TESTING AND BUILD_TESTING)
  # BUILD_TESTING gets disabled somewhere by some dependency. Rely on
  # CCGEN_BUILD_TESTING instead but still read BUILD_TESTING.
  set(CCGEN_BUILD_TESTING ON)
endif()

# Library

add_library(
  libccgen OBJECT
  src/base.h
  src/codec.h
  src/codec.cc
  src/codec_avif.h
  src/codec_avif.cc
  src/codec_avif_libheif.h
  src/codec_avif_libheif.cc
  src/codec_basis.h
  src/codec_basis.cc
  src/codec_combination.h
  src/codec_combination.cc
  src/codec_ffv1.h
  src/codec_ffv1.cc
  src/codec_jpegli.h
  src/codec_jpegli.cc
  src/codec_jpegmoz.h
  src/codec_jpegmoz.cc
  src/codec_jpegsimple.h
  src/codec_jpegsimple.cc
  src/codec_jpegturbo.h
  src/codec_jpegturbo.cc
  src/codec_jpegxl.h
  src/codec_jpegxl.cc
  src/codec_jpegzune.h
  src/codec_jpegzune.cc
  src/codec_openjpeg.h
  src/codec_openjpeg.cc
  src/codec_webp.h
  src/codec_webp.cc
  src/codec_webp_rs.h
  src/codec_webp_rs.cc
  src/codec_webp2.h
  src/codec_webp2.cc
  src/distortion.h
  src/distortion.cc
  src/frame.h
  src/frame.cc
  src/framework.h
  src/framework.cc
  src/result_json.h
  src/result_json.cc
  src/serialization.h
  src/serialization.cc
  src/task.h
  src/task.cc
  src/timer.h
  src/worker.h)
target_include_directories(libccgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT WIN32)
  target_compile_definitions(libccgen PRIVATE HAVE_UNISTD_H)
endif()

# Dependencies

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

if(CCGEN_WASM)
  set(CCGEN_ENABLE_PNG ON) # Could be an option for when CCGEN_WASM=OFF.
  include(LocalLibzlib)
  include(LocalLibpng)
else()
  set(CCGEN_ENABLE_PNG OFF)
endif()

if(CCGEN_ENABLE_JPEG)
  target_compile_definitions(libccgen PRIVATE HAS_JPEGMOZ)
  # mozjpeg (fork of turbojpeg) forbids add_subdirectory() and thus
  # FetchContent. It also forbids ExternalProject_Add() initiated from a
  # subdirectory. See
  # https://github.com/mozilla/mozjpeg/blob/08265790774cd0714832c9e675522acbe5581437/CMakeLists.txt#L36-L62

  set(mozjpeg_GIT_REPOSITORY "https://github.com/mozilla/mozjpeg.git")
  # Keep JpegmozVersion() in src/codec_jpegmoz.cc in sync with GIT_TAG.
  set(mozjpeg_GIT_TAG "08265790774cd0714832c9e675522acbe5581437") # 5.0.X

  include(ExternalProject)
  if(CCGEN_WASM)
    ExternalProject_Add(
      mozjpeg
      GIT_REPOSITORY ${mozjpeg_GIT_REPOSITORY}
      GIT_TAG ${mozjpeg_GIT_TAG}
      PREFIX ${CMAKE_BINARY_DIR}/_deps
      CONFIGURE_COMMAND
        emcmake ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} -S <SOURCE_DIR> -B
        <BINARY_DIR> -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS} -DPNG_SUPPORTED=FALSE
        -DWITH_TURBOJPEG=FALSE -DWITH_SIMD=FALSE
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      BUILD_COMMAND emmake ${CMAKE_COMMAND} --build <BINARY_DIR> --config
                    $<CONFIG> --parallel
      INSTALL_COMMAND ""
      BUILD_BYPRODUCTS <BINARY_DIR>/${CCGEN_PREFIX}jpeg${CCGEN_SUFFIX})
    add_library(jpeg STATIC IMPORTED GLOBAL)
    add_dependencies(jpeg mozjpeg)
    set_target_properties(
      jpeg
      PROPERTIES
        IMPORTED_LOCATION
        ${CMAKE_BINARY_DIR}/_deps/src/mozjpeg-build/${CCGEN_PREFIX}jpeg${CCGEN_SUFFIX}
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}/_deps/src/mozjpeg
        ${CMAKE_BINARY_DIR}/_deps/src/mozjpeg-build
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
        ${CMAKE_BINARY_DIR}/_deps/src/mozjpeg
        ${CMAKE_BINARY_DIR}/_deps/src/mozjpeg-build)
  else()
    ExternalProject_Add(
      mozjpeg
      GIT_REPOSITORY ${mozjpeg_GIT_REPOSITORY}
      GIT_TAG ${mozjpeg_GIT_TAG}
      PREFIX ${CMAKE_BINARY_DIR}/_deps
      CMAKE_ARGS -DPNG_SUPPORTED=FALSE -DWITH_TURBOJPEG=FALSE
                 -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                 -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
      INSTALL_COMMAND ""
      BUILD_BYPRODUCTS <BINARY_DIR>/${CCGEN_PREFIX}jpeg${CCGEN_SUFFIX})
  endif()
  target_include_directories(
    libccgen PRIVATE ${CMAKE_BINARY_DIR}/_deps/src/mozjpeg
                     ${CMAKE_BINARY_DIR}/_deps/src/mozjpeg-build)
  target_link_libraries(
    libccgen
    ${CMAKE_BINARY_DIR}/_deps/src/mozjpeg-build/${CCGEN_PREFIX}jpeg${CCGEN_SUFFIX}
  )

  # Skip sjpeg's find_package(JPEG).
  include(FetchContent)
  FetchContent_Declare(JPEG DOWNLOAD_COMMAND "" OVERRIDE_FIND_PACKAGE SYSTEM)
  set(JPEG_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/_deps/src/mozjpeg
                        ${CMAKE_BINARY_DIR}/_deps/src/mozjpeg-build)
  set(JPEG_LIBRARIES jpeg)

  target_compile_definitions(libccgen PRIVATE HAS_JPEGSIMPLE)
  include(LocalSjpeg)
  target_link_libraries(libccgen sjpeg)

  target_compile_definitions(libccgen PRIVATE HAS_JPEGTURBO)
  # libjpeg-turbo forbids add_subdirectory() and thus FetchContent. It also
  # forbids ExternalProject_Add() initiated from a subdirectory. See
  # https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/CMakeLists.txt#L45-L71

  set(libjpeg_turbo_GIT_REPOSITORY
      "https://github.com/libjpeg-turbo/libjpeg-turbo.git")
  set(libjpeg_turbo_GIT_TAG "3.2.0")

  include(ExternalProject)
  if(CCGEN_WASM)
    ExternalProject_Add(
      libjpeg_turbo
      GIT_REPOSITORY ${libjpeg_turbo_GIT_REPOSITORY}
      GIT_TAG ${libjpeg_turbo_GIT_TAG}
      PREFIX ${CMAKE_BINARY_DIR}/_deps
      CONFIGURE_COMMAND
        emcmake ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} -S <SOURCE_DIR> -B
        <BINARY_DIR> -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
        -DENABLE_SHARED=${BUILD_SHARED_LIBS}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DWITH_TURBOJPEG=TRUE
        -DWITH_SIMD=FALSE
      BUILD_COMMAND emmake ${CMAKE_COMMAND} --build <BINARY_DIR> --config
                    $<CONFIG> --parallel
      INSTALL_COMMAND ""
      BUILD_BYPRODUCTS <BINARY_DIR>/${CCGEN_PREFIX}turbojpeg${CCGEN_SUFFIX})
  else()
    ExternalProject_Add(
      libjpeg_turbo
      GIT_REPOSITORY ${libjpeg_turbo_GIT_REPOSITORY}
      GIT_TAG ${libjpeg_turbo_GIT_TAG}
      PREFIX ${CMAKE_BINARY_DIR}/_deps
      CMAKE_ARGS -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
                 -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      INSTALL_COMMAND ""
      BUILD_BYPRODUCTS <BINARY_DIR>/${CCGEN_PREFIX}turbojpeg${CCGEN_SUFFIX})
  endif()
  target_include_directories(
    libccgen PRIVATE ${CMAKE_BINARY_DIR}/_deps/src/libjpeg_turbo
                     ${CMAKE_BINARY_DIR}/_deps/src/libjpeg_turbo-build)
  target_link_libraries(
    libccgen
    ${CMAKE_BINARY_DIR}/_deps/src/libjpeg_turbo-build/${CCGEN_PREFIX}turbojpeg${CCGEN_SUFFIX}
  )
endif()

if(CCGEN_ENABLE_AVIF)
  include(LocalLibavif)
  target_compile_definitions(libccgen PRIVATE HAS_AVIF)
  target_link_libraries(libccgen avif)
endif()
if(CCGEN_ENABLE_AVIF_LIBHEIF)
  if(NOT CCGEN_ENABLE_AVIF)
    message(
      FATAL_ERROR "CCGEN_ENABLE_AVIF_LIBHEIF=ON requires CCGEN_ENABLE_AVIF=ON.")
  endif()
  include(LocalLibheif)
  target_compile_definitions(libccgen PRIVATE HAS_HEIF)
  target_link_libraries(libccgen heif)
endif()

if(CCGEN_ENABLE_WEBP)
  include(LocalLibwebp)
  target_compile_definitions(libccgen PRIVATE HAS_WEBP)
  target_link_libraries(libccgen webpmux webpdemux webp)
endif()

if(CCGEN_ENABLE_JPEGZUNE OR CCGEN_ENABLE_WEBPRS)
  if(CCGEN_WASM)
    message(FATAL_ERROR "Rust bindings are not supported with CCGEN_WASM=ON.")
  endif()

  # Copy the files from CMAKE_SOURCE_DIR to CMAKE_BINARY_DIR to avoid generating
  # build files outside of CMAKE_BINARY_DIR.
  configure_file(bindings/Cargo.toml bindings/Cargo.toml COPYONLY)
  configure_file(bindings/lib.rs bindings/lib.rs COPYONLY)

  # Corrosion is used to call cargo from CMake.
  include(FetchContent)
  FetchContent_Declare(
    Corrosion
    EXCLUDE_FROM_ALL
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.6.1
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(Corrosion)
  corrosion_import_crate(MANIFEST_PATH ${CMAKE_BINARY_DIR}/bindings/Cargo.toml)
  target_link_libraries(libccgen ccgen_webp_rs)

  if(CCGEN_ENABLE_JPEGZUNE)
    target_compile_definitions(libccgen PRIVATE HAS_JPEGZUNE)
  endif()

  if(CCGEN_ENABLE_WEBPRS)
    target_compile_definitions(libccgen PRIVATE HAS_WEBPRS)
  endif()
endif()

if(CCGEN_ENABLE_WEBP2)
  include(LocalLibwebp2)
  target_link_libraries(libccgen webp2 imageio)
  if(CCGEN_ENABLE_WEBP)
    # libccgen also uses WebP encoding and decoding through libwebp2.
    target_compile_definitions(libccgen PRIVATE WP2_HAVE_WEBP)
  endif()

  # Hardcode this include path so that libwebp2/src/utils/utils.h is included
  # instead of libwebp/src/utils/utils.h.
  target_include_directories(libccgen PRIVATE ${libwebp2_SOURCE_DIR})
else()
  message(FATAL_ERROR "codec-compare-gen requires CCGEN_ENABLE_WEBP2=ON.")
endif()

if(CCGEN_ENABLE_JPEG2000)
  target_compile_definitions(libccgen PRIVATE HAS_OPENJPEG)
  # FetchContent_MakeAvailable() results in weird build mixups such as:
  #
  # No rule to make target '_deps/libaom-build/libaom.a', needed by
  # '_deps/libopenjpeg-build/bin/heif-test'.
  #
  # But there is no heif-test in libopenjpeg. Rely on ExternalProject_Add()
  # instead.
  include(ExternalProject)
  ExternalProject_Add(
    libopenjpeg
    GIT_REPOSITORY https://github.com/uclouvain/openjpeg.git
    GIT_TAG v2.5.4
    PREFIX ${CMAKE_BINARY_DIR}/_deps
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS <BINARY_DIR>/bin/${CCGEN_PREFIX}openjp2${CCGEN_SUFFIX})
  # For openjpeg.h.
  target_include_directories(
    libccgen PRIVATE ${CMAKE_BINARY_DIR}/_deps/src/libopenjpeg/src/lib/openjp2)
  # For opj_config.h included by openjpeg.h.
  target_include_directories(
    libccgen
    PRIVATE ${CMAKE_BINARY_DIR}/_deps/src/libopenjpeg-build/src/lib/openjp2)
  target_link_libraries(
    libccgen
    ${CMAKE_BINARY_DIR}/_deps/src/libopenjpeg-build/bin/${CCGEN_PREFIX}openjp2${CCGEN_SUFFIX}
  )
endif()

if(CCGEN_ENABLE_FFV1)
  target_compile_definitions(libccgen PRIVATE HAS_FFV1)
  include(ExternalProject)
  ExternalProject_Add(
    libavcodec
    GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
    GIT_TAG n8.1
    PREFIX ${CMAKE_BINARY_DIR}/_deps
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<BINARY_DIR>
                      --enable-shared
    BUILD_COMMAND make libavcodec -j
    INSTALL_COMMAND make install libavcodec -j
    BUILD_BYPRODUCTS <BINARY_DIR>/lib/${CCGEN_PREFIX}avcodec${CCGEN_SUFFIX}
                     <BINARY_DIR>/lib/${CCGEN_PREFIX}avutil${CCGEN_SUFFIX})
  target_include_directories(
    libccgen PRIVATE ${CMAKE_BINARY_DIR}/_deps/src/libavcodec-build/include)
  target_link_libraries(
    libccgen
    ${CMAKE_BINARY_DIR}/_deps/src/libavcodec-build/lib/${CCGEN_PREFIX}avcodec${CCGEN_SUFFIX}
    ${CMAKE_BINARY_DIR}/_deps/src/libavcodec-build/lib/${CCGEN_PREFIX}avutil${CCGEN_SUFFIX}
  )
  if(NOT BUILD_SHARED_LIBS)
    target_link_libraries(libccgen lzma swresample)
  endif()
  # Avoid race conditions leading to "No rule to make libavcodec" errors.
  add_dependencies(libccgen libavcodec)
endif()

if(CCGEN_ENABLE_JPEGXL)
  include(LocalLibjxl)
  target_compile_definitions(libccgen PRIVATE HAS_JPEGXL)
  target_link_libraries(libccgen jxl)
elseif(NOT CCGEN_WASM)
  message(
    WARNING
      "codec-compare-gen requires CCGEN_ENABLE_JPEGXL=ON when comparing lossy codecs."
  )
endif()

if(CCGEN_ENABLE_JPEGLI)
  include(LocalJpegli)
  target_compile_definitions(libccgen PRIVATE HAS_JPEGLI)
  target_link_libraries(libccgen jpegli-static)
endif()

# Metric binaries called by ccgen because the API is missing or weird.
if(CCGEN_ENABLE_DSSIM)
  if(CCGEN_WASM)
    message(
      FATAL_ERROR "CCGEN_ENABLE_DSSIM=ON is not supported with CCGEN_WASM=ON.")
  endif()
  include(LocalDssim)
  add_dependencies(libccgen dssim_binary)
elseif(NOT CCGEN_WASM)
  message(
    WARNING
      "codec-compare-gen requires CCGEN_ENABLE_DSSIM=ON when comparing lossy codecs."
  )
endif()

if(CCGEN_ENABLE_JPEGXL)
  # These are built alongside libjxl. Reference them here anyway.
  add_dependencies(libccgen ssimulacra_main)
  add_dependencies(libccgen ssimulacra2)
  add_dependencies(libccgen butteraugli_main) # Also used for P3-norm.
endif()

if(CCGEN_ENABLE_BASIS)
  target_compile_definitions(libccgen PRIVATE HAS_BASIS)
  include(LocalBasisuniversal)
  target_link_libraries(libccgen basisu_encoder)
endif()

# WASM

if(CCGEN_WASM)
  if(NOT EMSCRIPTEN_VERSION)
    message(WARNING "EMSCRIPTEN_VERSION not detected")
  endif()

  add_executable(codec_wasm wasm/codec_wasm.cc)
  target_include_directories(codec_wasm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(codec_wasm libccgen)
  set_target_properties(codec_wasm PROPERTIES OUTPUT_NAME "codec_wasm_bin")
  set_target_properties(codec_wasm PROPERTIES SUFFIX ".js")
  target_link_options(
    codec_wasm PRIVATE "-sMODULARIZE=1" "-sEXPORT_NAME=loadCodecWasm"
    "-sENVIRONMENT=node,worker" "--bind")
  # Making the following work would be nicer for TypeScript integration:
  # "--emit-tsd=${CMAKE_CURRENT_BINARY_DIR}/codec_wasm_bin.d.ts"
endif()

# Tools

if(NOT CCGEN_WASM)
  add_executable(ccgen tools/ccgen_impl.cc tools/ccgen.cc)
  target_include_directories(ccgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(ccgen libccgen)

  add_executable(are_images_equivalent tools/are_images_equivalent.cc)
  target_include_directories(are_images_equivalent
                             PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(are_images_equivalent libccgen)

  add_executable(strip_metadata tools/strip_metadata.cc)
  target_include_directories(strip_metadata PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(strip_metadata libccgen)
endif()

# Tests

if(CCGEN_BUILD_TESTING)
  enable_testing()
  if(CCGEN_WASM)
    # Create a node.js package so that run npm i and npm run test can be used.
    configure_file(wasm/package.json package.json COPYONLY)
    configure_file(wasm/tsconfig.json tsconfig.json COPYONLY)
    configure_file(wasm/tests/web-test-runner.config.mjs
                   web-test-runner.config.mjs COPYONLY)
    add_custom_command(
      TARGET codec_wasm
      POST_BUILD
      COMMAND npm i
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    configure_file(wasm/tests/test_wasm_test.ts test_wasm_test.ts COPYONLY)
    configure_file(tests/data/gradient32x32.png gradient32x32.png COPYONLY)
    add_test(
      NAME test_wasm_test
      COMMAND npm run test
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  else()
    find_package(GTest REQUIRED)

    macro(add_ccgen_gtest TEST_NAME)
      add_executable(${TEST_NAME} tools/ccgen_impl.cc tests/${TEST_NAME}.cc)
      target_include_directories(${TEST_NAME}
                                 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
      target_link_libraries(${TEST_NAME} PRIVATE libccgen GTest::gtest)
      if(${ARGC} EQUAL 3)
        add_test(NAME ${TEST_NAME}
                 COMMAND ${TEST_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/${ARGV1}/
                         ${ARGV2}/)
      elseif(${ARGC} EQUAL 2)
        add_test(NAME ${TEST_NAME}
                 COMMAND ${TEST_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/${ARGV1}/)
      else()
        target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
      endif()
    endmacro()

    add_ccgen_gtest(test_ccgen tests/data)
    add_ccgen_gtest(test_codec tests/data)
    add_ccgen_gtest(test_codec_avif)
    if(CCGEN_ENABLE_JPEGZUNE)
      add_ccgen_gtest(test_codec_jpegzune tests/data)
    endif()
    add_ccgen_gtest(test_codec_sjpeg tests/data)
    if(CCGEN_ENABLE_WEBPRS)
      add_ccgen_gtest(test_codec_webp_rs tests/data)
    endif()
    add_ccgen_gtest(test_distortion tests/data ${CMAKE_CURRENT_BINARY_DIR})
    add_ccgen_gtest(test_framework tests/data)
    add_ccgen_gtest(test_serialization)
    add_ccgen_gtest(test_task)
    add_ccgen_gtest(test_worker)
  endif()
endif()
