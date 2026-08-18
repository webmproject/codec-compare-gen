#!/bin/bash

# Copyright 2026 Google LLC
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

set -eux

if [[ "$#" -ge 1 ]]; then
  SOURCE_DIR="${1}"
else
  # Assume this was called from the root directory of the codec-compare-gen
  # repository.
  SOURCE_DIR="."
fi

if [[ "$#" -ge 2 ]]; then
  BUILD_DIR="${2}"
else
  BUILD_DIR="${SOURCE_DIR}/build_WASM"
fi

if [[ "$#" -ge 3 ]]; then
  BUILD_TESTING="${3}"
else
  BUILD_TESTING="OFF"
fi

NUM_THREADS=$(($(nproc) - 1))

emcmake cmake \
  -S "${SOURCE_DIR}" \
  -B "${BUILD_DIR}" \
  -DCCGEN_ENABLE_AVIF=OFF \
  -DCCGEN_ENABLE_AVIF_LIBHEIF=OFF \
  -DCCGEN_ENABLE_JPEG=ON \
  -DCCGEN_ENABLE_JPEGLI=OFF \
  -DCCGEN_ENABLE_JPEG2000=OFF \
  -DCCGEN_ENABLE_JPEGXL=OFF \
  -DCCGEN_ENABLE_WEBP=ON \
  -DCCGEN_ENABLE_WEBPRS=OFF \
  -DCCGEN_ENABLE_WEBP2=ON \
  -DCCGEN_ENABLE_FFV1=OFF \
  -DCCGEN_ENABLE_BASIS=OFF \
  -DCCGEN_ENABLE_DSSIM=OFF \
  -DCCGEN_WASM=ON \
  -DCCGEN_BUILD_TESTING="${BUILD_TESTING}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5

emmake cmake \
  --build "${BUILD_DIR}" \
  -j${NUM_THREADS}
