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

#ifndef SRC_DISTORTION_H_
#define SRC_DISTORTION_H_

#include <cstddef>
#include <string>

#include "src/base.h"
#include "src/frame.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

namespace codec_compare_gen {

// Computes the average distortion between the given frame sequences.
// They must have the same total duration.
StatusOr<float> GetAverageDistortion(
    const std::string& a_path, const Image& a, const std::string& b_path,
    const Image& b, const TaskInput& task,
    const std::string& metric_binary_folder_path, DistortionMetric metric,
    size_t thread_id, bool quiet);

// Returns true if all pixels match between the two given frame sequences.
// They must have the same total duration.
StatusOr<bool> PixelEquality(const Image& a, const Image& b, bool quiet);

#if defined(HAS_WEBP2)
StatusOr<bool> PixelEquality(const WP2::ArgbBuffer& a, const WP2::ArgbBuffer& b,
                             bool quiet);
#endif

}  // namespace codec_compare_gen

#endif  // SRC_DISTORTION_H_
