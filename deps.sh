#!/bin/bash
# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Required dependencies:
#   sudo apt install cmake meson ninja-build clang yasm libpng-dev libjpeg-dev cargo rustc
# Required dependencies for BUILD_TESTING=ON:
#   sudo apt install libgtest-dev

set -e

mkdir third_party
pushd third_party

  git clone https://github.com/AOMediaCodec/libavif.git
  pushd libavif
    git checkout cdb1c3df9f3e02b2bbe2686474440a081ebb9b6a
    pushd ext
      ./aom.cmd
      ./dav1d.cmd
      ./libsharpyuv.cmd
      ./libyuv.cmd
    popd
    cmake -S . -B build \
      -DAVIF_BUILD_APPS=ON \
      -DAVIF_BUILD_EXAMPLES=OFF \
      -DAVIF_BUILD_TESTS=OFF \
      -DAVIF_CODEC_AOM=LOCAL \
      -DAVIF_CODEC_DAV1D=LOCAL \
      -DAVIF_LIBYUV=LOCAL \
      -DAVIF_LIBSHARPYUV=LOCAL \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build --parallel
  popd

  git clone https://chromium.googlesource.com/webm/libwebp
  pushd libwebp
    git checkout 1fb9f3dcf18b2aff35dcb7a8f6fa4a6c756efb75
    cmake -S . -B build \
      -DWEBP_BUILD_CWEBP=ON \
      -DWEBP_BUILD_DWEBP=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build --parallel

    # This file creates errors when referenced by CMAKE_PREFIX_PATH below.
    mv build/WebPConfig.cmake build/WebPConfig.cmake.bck
  popd

  git clone https://chromium.googlesource.com/codecs/libwebp2
  pushd libwebp2
    git checkout 543e052dc2673dfa3d18bc9d511c8870316d3389
    cmake -S . -B build \
      -DCMAKE_PREFIX_PATH="../libwebp/src/;../libwebp/build/" \
      -DWP2_BUILD_TESTS=OFF \
      -DWP2_BUILD_EXTRAS=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build --parallel
  popd

  # DEVTOOLS=ON for Butteraugli and SSIMULACRA2 metrics binaries. See
  # https://github.com/cloudinary/ssimulacra2/blob/d2be72505ddc5c92aeb30f4a7f3ab53db45b314b/build_ssimulacra_from_libjxl_repo
  git clone https://github.com/libjxl/libjxl.git
  pushd libjxl
    # https://github.com/libjxl/libjxl/releases/tag/v0.10.0
    git checkout 19bcd827dbf44bdc7ef30af8925696a5b82dcdf2
    ./deps.sh
    # DEVTOOLS=ON for metric binaries. See
    # https://github.com/cloudinary/ssimulacra2/blob/d2be72505ddc5c92aeb30f4a7f3ab53db45b314b/build_ssimulacra_from_libjxl_repo
    cmake -S . -B build \
      -DBUILD_TESTING=OFF \
      -DJPEGXL_ENABLE_BENCHMARK=OFF \
      -DJPEGXL_ENABLE_EXAMPLES=OFF \
      -DJPEGXL_ENABLE_JPEGLI=OFF \
      -DJPEGXL_ENABLE_OPENEXR=OFF \
      -DJPEGXL_ENABLE_DEVTOOLS=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build --parallel
  popd

  git clone https://github.com/kornelski/dssim.git
  pushd dssim
    # https://github.com/kornelski/dssim/releases/tag/3.2.3
    git checkout 14995bc19a6ac75abf6e171cdfb17f26ad980879
    cargo build --release
  popd

popd
