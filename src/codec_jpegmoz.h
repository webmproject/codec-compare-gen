// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_CODEC_JPEGMOZ_H_
#define SRC_CODEC_JPEGMOZ_H_

#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

namespace codec_compare_gen {

std::string JpegmozVersion();

std::vector<int> JpegmozLossyQualities();

#if defined(HAS_WEBP2)
StatusOr<WP2::Data> EncodeJpegmoz(const TaskInput& input,
                                  const WP2::ArgbBuffer& original_image,
                                  bool quiet);
StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeJpegmoz(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet);
#endif  // HAS_WEBP2

}  // namespace codec_compare_gen

#endif  // SRC_CODEC_JPEGMOZ_H_