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
    git checkout a9ac378e84daec87dc7f6c438bf0215c6165de39
    cmake -S . -B build \
      -DAVIF_BUILD_APPS=ON \
      -DAVIF_BUILD_EXAMPLES=OFF \
      -DAVIF_BUILD_TESTS=OFF \
      -DAVIF_CODEC_AOM=LOCAL \
      -DAVIF_CODEC_DAV1D=LOCAL \
      -DAVIF_LIBYUV=LOCAL \
      -DAVIF_LIBSHARPYUV=LOCAL \
      -DAVIF_ENABLE_EXPERIMENTAL_YCGCO_R=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build --parallel
  popd

  git clone https://chromium.googlesource.com/webm/libwebp
  pushd libwebp
    git checkout a443170fc0ebdfc3abbf89ac81f35e7eb656a3da # v1.4.0
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
    git checkout 425411e8016acc4497d23999e168bf38970c3afb
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
    # https://github.com/libjxl/libjxl/releases/tag/v0.11.0
    git checkout 4df1e9eccdf86b8df4c0c7c08f529263906f9c4f
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
      -DJPEGXL_ENABLE_JPEGLI=ON -DJPEGXL_ENABLE_JPEGLI_LIBJPEG=OFF \
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

  git clone https://github.com/libjpeg-turbo/libjpeg-turbo.git libjpeg_turbo
  pushd libjpeg_turbo
    git checkout e287a35762cba20e2253efb3260007289a2f2186
    cmake -S . -B build
    cmake --build build --parallel
  popd

  git clone https://github.com/webmproject/sjpeg.git
  pushd sjpeg
    git checkout 4578abf18ed8b81290c6fe5c23eb7a58c8f38212
    cmake -S . -B build \
      -DSJPEG_BUILD_EXAMPLES=OFF \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build --parallel
  popd

  git clone https://github.com/mozilla/mozjpeg.git
  pushd mozjpeg
    git checkout 6c9f0897afa1c2738d7222a0a9ab49e8b536a267
    cmake -S . -B build -DWITH_TURBOJPEG=OFF
    cmake --build build --parallel
  popd

popd
