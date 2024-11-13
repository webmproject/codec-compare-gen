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
  VERSION 0.3.4)
set(CMAKE_CXX_STANDARD 17)

option(BUILD_SHARED_LIBS "Build the shared codec-compare-gen library" ON)
if(BUILD_SHARED_LIBS)
  set(CCGEN_PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}")
  set(CCGEN_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
else()
  set(CCGEN_PREFIX "${CMAKE_STATIC_LIBRARY_PREFIX}")
  set(CCGEN_SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

set(CCGEN_TD "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

add_library(
  libccgen OBJECT
  src/base.h
  src/codec.h
  src/codec.cc
  src/codec_avif.h
  src/codec_avif.cc
  src/codec_combination.h
  src/codec_combination.cc
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
  src/codec_webp.h
  src/codec_webp.cc
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

# Dependencies

target_compile_definitions(libccgen PRIVATE HAS_WEBP2)
target_include_directories(libccgen PRIVATE ${CCGEN_TD}/libwebp2)
target_link_directories(libccgen PRIVATE ${CCGEN_TD}/libwebp2/build)
target_link_libraries(
  libccgen ${CCGEN_TD}/libwebp2/build/${CCGEN_PREFIX}webp2${CCGEN_SUFFIX})
target_link_libraries(
  libccgen ${CCGEN_TD}/libwebp2/build/${CCGEN_PREFIX}imageio${CCGEN_SUFFIX})

target_compile_definitions(libccgen PRIVATE HAS_AVIF)
target_include_directories(libccgen PRIVATE ${CCGEN_TD}/libavif/include)
target_link_directories(libccgen PRIVATE ${CCGEN_TD}/libavif/build)
target_link_libraries(
  libccgen ${CCGEN_TD}/libavif/build/${CCGEN_PREFIX}avif${CCGEN_SUFFIX})

target_compile_definitions(libccgen PRIVATE HAS_WEBP)
target_include_directories(libccgen PRIVATE ${CCGEN_TD}/libwebp/src)
target_link_directories(libccgen PRIVATE ${CCGEN_TD}/libwebp/build)
target_link_libraries(
  libccgen ${CCGEN_TD}/libwebp/build/${CCGEN_PREFIX}webpmux${CCGEN_SUFFIX}
  ${CCGEN_TD}/libwebp/build/${CCGEN_PREFIX}webpdemux${CCGEN_SUFFIX}
  ${CCGEN_TD}/libwebp/build/${CCGEN_PREFIX}webp${CCGEN_SUFFIX})
# libccgen also uses WebP encoding and decoding through libwebp2.
target_compile_definitions(libccgen PRIVATE WP2_HAVE_WEBP)

target_compile_definitions(libccgen PRIVATE HAS_JPEGXL)
target_include_directories(libccgen
                           PRIVATE ${CCGEN_TD}/libjxl/build/lib/include)
target_link_directories(libccgen PRIVATE ${CCGEN_TD}/libjxl/build/lib)
target_link_libraries(
  libccgen ${CCGEN_TD}/libjxl/build/lib/${CCGEN_PREFIX}jxl${CCGEN_SUFFIX})
# jpegli is part of libjxl. For lib/jpegli/types.h included by common.h included
# by codec_jpegli.cc:
target_include_directories(libccgen PRIVATE ${CCGEN_TD}/libjxl)
# libjxl does not generate any shared libjpegli binary. Using the libjpeg
# drop-in replacement would result in symbol collisions with other similar
# implementations. Use the static binary and its dependency instead:
target_link_libraries(
  libccgen
  ${CCGEN_TD}/libjxl/build/lib/${CMAKE_STATIC_LIBRARY_PREFIX}jpegli-static${CMAKE_STATIC_LIBRARY_SUFFIX}
  ${CCGEN_TD}/libjxl/build/third_party/highway/${CMAKE_STATIC_LIBRARY_PREFIX}hwy${CMAKE_STATIC_LIBRARY_SUFFIX}
)

target_compile_definitions(libccgen PRIVATE HAS_JPEGMOZ)
target_link_directories(libccgen PRIVATE ${CCGEN_TD}/mozjpeg/build)
target_link_libraries(
  libccgen ${CCGEN_TD}/mozjpeg/build/${CCGEN_PREFIX}jpeg${CCGEN_SUFFIX})

target_compile_definitions(libccgen PRIVATE HAS_JPEGSIMPLE)
target_link_directories(libccgen PRIVATE ${CCGEN_TD}/sjpeg/build)
target_link_libraries(
  libccgen ${CCGEN_TD}/sjpeg/build/${CCGEN_PREFIX}sjpeg${CCGEN_SUFFIX})

target_compile_definitions(libccgen PRIVATE HAS_JPEGTURBO)
target_include_directories(libccgen PRIVATE ${CCGEN_TD}/libjpeg_turbo
                                            ${CCGEN_TD}/libjpeg_turbo/build)
target_link_directories(libccgen PRIVATE ${CCGEN_TD}/libjpeg_turbo/build)
target_link_libraries(
  libccgen
  ${CCGEN_TD}/libjpeg_turbo/build/${CCGEN_PREFIX}turbojpeg${CCGEN_SUFFIX})

# Tools

add_executable(ccgen tools/ccgen_impl.cc tools/ccgen.cc)
target_include_directories(ccgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ccgen libccgen)

# Tests

option(BUILD_TESTING "Build the tests (requires GoogleTest)" OFF)
if(BUILD_TESTING)
  find_package(GTest REQUIRED)
  enable_testing()

  macro(add_ccgen_gtest TEST_NAME)
    add_executable(${TEST_NAME} tools/ccgen_impl.cc tests/${TEST_NAME}.cc)
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${TEST_NAME} PRIVATE libccgen GTest::gtest)
    target_compile_definitions(${TEST_NAME} PRIVATE HAS_WEBP2)
    if(${ARGC} EQUAL 2)
      add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
                                         ${CMAKE_CURRENT_SOURCE_DIR}/${ARGV1}/)
    else()
      target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main)
      add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endif()
  endmacro()

  add_ccgen_gtest(test_ccgen tests/data)
  add_ccgen_gtest(test_codec tests/data)
  add_ccgen_gtest(test_codec_avif)
  add_ccgen_gtest(test_codec_sjpeg tests/data)
  add_ccgen_gtest(test_distortion tests/data)
  add_ccgen_gtest(test_framework tests/data)
  add_ccgen_gtest(test_serialization)
  add_ccgen_gtest(test_task)
  add_ccgen_gtest(test_worker)
endif()
