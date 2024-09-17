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

#ifndef SRC_FRAME_H_
#define SRC_FRAME_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "src/base.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

namespace codec_compare_gen {

struct Frame {
#if defined(HAS_WEBP2)
  Frame() = default;
  Frame(Frame&&) = default;
  Frame(WP2::ArgbBuffer&& pixels, uint32_t duration_ms)
      : pixels(std::move(pixels)), duration_ms(duration_ms) {};

  WP2::ArgbBuffer pixels;
#endif
  uint32_t duration_ms;  // 0 for still images.
};

// Still or animated image.
using Image = std::vector<Frame>;

uint32_t GetDurationMs(const Image& image);

#if defined(HAS_WEBP2)

// Makes a deep copy of the given frame sequence and converts the pixels to the
// given format.
StatusOr<Image> CloneAs(const Image& from, WP2SampleFormat format, bool quiet);

// Makes a shallow copy of the given frame sequence.
StatusOr<Image> MakeView(const Image& from, bool quiet);

// Reads a file into a frame sequence.
StatusOr<Image> ReadStillImageOrAnimation(const char* file_path,
                                          WP2SampleFormat format, bool quiet);

// Writes a frame sequence to a file (PNG for still images, WebP for
// animations).
Status WriteStillImageOrAnimation(const Image& image, const char* file_path,
                                  bool quiet);

#endif

}  // namespace codec_compare_gen

#endif  // SRC_FRAME_H_
