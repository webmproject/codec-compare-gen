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

  git clone -b v1.4.0 --depth 1 https://github.com/AOMediaCodec/libavif.git
  pushd libavif
    git checkout d145e1a32af2915779b27e3b0521b6db08dd6bb8 # v1.4.0
    cmake -S . -B build \
      -DAVIF_BUILD_APPS=ON \
      -DAVIF_BUILD_EXAMPLES=OFF \
      -DAVIF_BUILD_TESTS=OFF \
      -DAVIF_CODEC_AOM=LOCAL \
      -DAVIF_CODEC_DAV1D=LOCAL \
      -DAVIF_CODEC_AVM=LOCAL \
      -DAVIF_LIBYUV=LOCAL \
      -DAVIF_LIBSHARPYUV=LOCAL \
      -DAVIF_ENABLE_EXPERIMENTAL_MINI=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    cmake --build build -j${NPROC}
  popd

  git clone -b v1.21.2 --depth 1 https://github.com/strukturag/libheif.git
  pushd libheif
    git checkout 62f1b8c76ed4d8305071fdacbe74ef9717bacac5 # v1.21.2
    # Reuse the libaom and dav1d dependencies from libavif.
    # pushd third-party
    #   chmod +x *.cmd
    #   ./aom.cmd
    #   ./dav1d.cmd
    # popd
    cmake -S . -B build \
      -DBUILD_TESTING=OFF \
      -DENABLE_PLUGIN_LOADING=OFF \
      -DWITH_AOM_DECODER=OFF \
      -DWITH_AOM_ENCODER=ON \
      -DAOM_INCLUDE_DIR=../libavif/build/_deps/libaom-src/ \
      -DAOM_LIBRARY=../libavif/build/_deps/aom-build/libaom.a \
      -DWITH_DAV1D=ON \
      -DDAV1D_INCLUDE_DIR=../libavif/build/_deps/dav1d-src/include/ \
      -DDAV1D_LIBRARY=../libavif/build/_deps/dav1d-build/src/libdav1d.a \
      -DWITH_LIBSHARPYUV=OFF \
      -DENABLE_MULTITHREADING_SUPPORT=OFF \
      -DENABLE_PARALLEL_TILE_DECODING=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build -j${NPROC}
  popd

  git clone -b v1.6.0 --depth 1 https://chromium.googlesource.com/webm/libwebp
  pushd libwebp
    git checkout b7e29b9d75bd31422b00c2a446d49d7af06c328d # v1.6.0
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
    git checkout 8720150cdc4c5c51a11a809a93110f38035b6048
    cmake -S . -B build \
      -DCMAKE_PREFIX_PATH="../libwebp/src/;../libwebp/build/" \
      -DWP2_BUILD_TESTS=OFF \
      -DWP2_BUILD_EXTRAS=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build -j${NPROC}
  popd

  git clone -b v0.11.2 --depth 1 https://github.com/libjxl/libjxl.git
  pushd libjxl
    git checkout 332feb17d17311c748445f7ee75c4fb55cc38530 # v0.11.2
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

  git clone -b v2.5.4 --depth 1 https://github.com/uclouvain/openjpeg.git
  pushd openjpeg
    git checkout 6c4a29b00211eb0430fa0e5e890f1ce5c80f409f # v2.5.4
    cmake -S . -B build \
      -DBUILD_TESTING=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang \
      -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF
    cmake --build build -j${NPROC}
  popd

  # FFV1 is part of FFmpeg.
  git clone -b n8.1 --depth 1 https://github.com/FFmpeg/FFmpeg.git
  pushd FFmpeg
    git checkout 9047fa1b084f76b1b4d065af2d743df1b40dfb56 # n8.1
    ./configure --prefix=build --enable-shared --disable-static
    make libavcodec -j${NPROC}
    make install libavcodec -j${NPROC}
  popd

  git clone -b v2_1_0 --depth 1 https://github.com/BinomialLLC/basis_universal.git
  pushd basis_universal
    git checkout 45d5f41015eecd9570d5a3f89ab9cc0037a25063 # v2.10
    cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release -DBASISU_SSE=TRUE \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_SHARED_LIBS=ON -DBASISU_STATIC=OFF
    cmake --build build -j${NPROC}
  popd

  git clone https://github.com/kornelski/dssim.git
  pushd dssim
    git checkout c86745c423478993a12edf59ec76047ff52b3da4 # 3.4.0
    cargo build --release
  popd

  git clone -b 3.1.3 --depth 1 https://github.com/libjpeg-turbo/libjpeg-turbo.git libjpeg_turbo
  pushd libjpeg_turbo
    git checkout af9c1c268520a29adf98cad5138dafe612b3d318 # 3.1.3
    cmake -S . -B build
    cmake --build build -j${NPROC}
  popd

  git clone https://github.com/webmproject/sjpeg.git
  pushd sjpeg
    git checkout 46da5aec5fce05faabf1facf0066e36e6b1c4dff
    cmake -S . -B build \
      -DSJPEG_BUILD_EXAMPLES=OFF \
      -DBUILD_SHARED_LIBS=ON
    cmake --build build -j${NPROC}
  popd

  git clone https://github.com/mozilla/mozjpeg.git
  pushd mozjpeg
    git checkout 08265790774cd0714832c9e675522acbe5581437 # 5.0.X
    cmake -S . -B build -DWITH_TURBOJPEG=OFF \
      -DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    cmake --build build -j${NPROC}
  popd

popd
