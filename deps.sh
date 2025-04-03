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

NPROC=$(nproc)

mkdir third_party
pushd third_party

  git clone https://github.com/AOMediaCodec/libavif.git
  pushd libavif
    git checkout a28899ac7df6b211dae7199cb723d8ce2cecff1b # v1.2.0
    cmake -S . -B build \
      -DAVIF_BUILD_APPS=ON \
      -DAVIF_BUILD_EXAMPLES=OFF \
      -DAVIF_BUILD_TESTS=OFF \
      -DAVIF_CODEC_AOM=LOCAL \
      -DAVIF_CODEC_DAV1D=LOCAL \
      -DAVIF_LIBYUV=LOCAL \
      -DAVIF_LIBSHARPYUV=LOCAL \
      -DAVIF_ENABLE_EXPERIMENTAL_MINI=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build -j${NPROC}
  popd

  # AVM symbols conflict with AOM so another build of libavif is required.
  # See https://gitlab.com/AOMediaCodec/avm/-/issues/150.
  git clone https://github.com/AOMediaCodec/libavif.git libavif_avm
  pushd libavif_avm
    git checkout ccfc16016ed264be3223a4649f61cd5f02807fe3 # just after v1.2.0
    # From https://github.com/AOMediaCodec/libavif/pull/2624.
    sed -i'' -e 's|set(CONFIG_ML_PART_SPLIT 0 CACHE INTERNAL "")|include_directories(${CMAKE_CURRENT_BINARY_DIR}/flatbuffers/include/)|' "cmake/Modules/LocalAom.cmake"
    sed -i'' -e 's|set_property(TARGET aom PROPERTY AVIF_LOCAL ON)|if(AVIF_CODEC_AVM)\ntarget_link_libraries(aom PRIVATE tensorflow-lite)\nendif()\nset_property(TARGET aom PROPERTY AVIF_LOCAL ON)|' "cmake/Modules/LocalAom.cmake"
    cmake -S . -B build \
      -DAVIF_BUILD_APPS=ON \
      -DAVIF_BUILD_EXAMPLES=OFF \
      -DAVIF_BUILD_TESTS=OFF \
      -DAVIF_CODEC_AVM=LOCAL \
      -DAVIF_LIBYUV=LOCAL \
      -DAVIF_LIBSHARPYUV=LOCAL \
      -DAVIF_ENABLE_EXPERIMENTAL_MINI=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build -j${NPROC}
  popd

  git clone https://chromium.googlesource.com/webm/libwebp
  pushd libwebp
    git checkout a4d7a715337ded4451fec90ff8ce79728e04126c # v1.5.0
    cmake -S . -B build \
      -DWEBP_BUILD_CWEBP=ON \
      -DWEBP_BUILD_DWEBP=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build -j${NPROC}

    # This file creates errors when referenced by CMAKE_PREFIX_PATH below.
    mv build/WebPConfig.cmake build/WebPConfig.cmake.bck
  popd

  git clone https://chromium.googlesource.com/codecs/libwebp2
  pushd libwebp2
    git checkout 664da0e1806085223e9c0b3deb920ca6fb504667
    cmake -S . -B build \
      -DCMAKE_PREFIX_PATH="../libwebp/src/;../libwebp/build/" \
      -DWP2_BUILD_TESTS=OFF \
      -DWP2_BUILD_EXTRAS=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build -j${NPROC}
  popd

  git clone https://github.com/libjxl/libjxl.git
  pushd libjxl
    git checkout 794a5dcf0d54f9f0b20d288a12e87afb91d20dfc # v0.11.1
    ./deps.sh
    # DEVTOOLS=ON for Butteraugli and SSIMULACRA2 metric binaries. See
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
    cmake --build build -j${NPROC}
  popd

  git clone -b v2.5.3 --depth 1 https://github.com/uclouvain/openjpeg.git
  pushd openjpeg
    git checkout 210a8a5690d0da66f02d49420d7176a21ef409dc # v2.5.3
    cmake -S . -B build \
      -DBUILD_TESTING=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang \
      -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF
    cmake --build build -j${NPROC}
  popd

  git clone https://github.com/kornelski/dssim.git
  pushd dssim
    git checkout 14995bc19a6ac75abf6e171cdfb17f26ad980879 # 3.2.3
    cargo build --release
  popd

  git clone https://github.com/libjpeg-turbo/libjpeg-turbo.git libjpeg_turbo
  pushd libjpeg_turbo
    git checkout e287a35762cba20e2253efb3260007289a2f2186
    cmake -S . -B build
    cmake --build build -j${NPROC}
  popd

  git clone https://github.com/webmproject/sjpeg.git
  pushd sjpeg
    git checkout 4578abf18ed8b81290c6fe5c23eb7a58c8f38212
    cmake -S . -B build \
      -DSJPEG_BUILD_EXAMPLES=OFF \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build -j${NPROC}
  popd

  git clone https://github.com/mozilla/mozjpeg.git
  pushd mozjpeg
    git checkout 6c9f0897afa1c2738d7222a0a9ab49e8b536a267
    cmake -S . -B build -DWITH_TURBOJPEG=OFF
    cmake --build build -j${NPROC}
  popd

popd
