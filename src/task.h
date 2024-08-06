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

#ifndef SRC_TASK_H_
#define SRC_TASK_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "src/base.h"
#include "src/framework.h"

namespace codec_compare_gen {

struct TaskInput {
  CodecSettings codec_settings;
  std::string image_path;    // Original image file path.
  std::string encoded_path;  // Encoded image file path.
                             // Can be empty to avoid saving to disk.
};

bool operator==(const TaskInput& a, const TaskInput& b);

struct TaskOutput {
  TaskInput task_input;  // For convenience.

  uint32_t image_width;   // in pixels
  uint32_t image_height;  // in pixels
  uint32_t num_frames;
  size_t encoded_size;       // in bytes
  double encoding_duration;  // in seconds
  double decoding_duration;  // in seconds, color conversion inclusive
  double decoding_color_conversion_duration;  // in seconds

  float distortions[kNumDistortionMetrics];

  std::string Serialize() const;
  static StatusOr<TaskOutput> UnserializeNoDistortion(
      const std::string& serialized_task, bool quiet);
  static StatusOr<TaskOutput> Unserialize(const std::string& serialized_task,
                                          bool quiet);
};

StatusOr<std::vector<TaskInput>> PlanTasks(
    const std::vector<std::string>& image_paths,
    const ComparisonSettings& settings);

// Used for the map below.
bool operator<(const CodecSettings& a, const CodecSettings& b);

// Returns unique pairs of image,quality results grouped by codec,effort.
StatusOr<std::vector<std::vector<TaskOutput>>>
SplitByCodecSettingsAndAggregateByImageAndQuality(
    const std::vector<TaskOutput>& results, bool quiet);

}  // namespace codec_compare_gen

#endif  // SRC_TASK_H_
