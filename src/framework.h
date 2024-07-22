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

#ifndef SRC_FRAMEWORK_H_
#define SRC_FRAMEWORK_H_

#include <cstdint>
#include <string>
#include <vector>

#include "src/base.h"

namespace codec_compare_gen {

struct CodecSettings {
  Codec codec;
  Subsampling chroma_subsampling;
  int effort;
  int quality;  // kQualityLossless or in [0:100] (exact range depends on codec)
};

struct ComparisonSettings {
  std::vector<CodecSettings> codec_settings;
  std::string metric_binary_folder_path;
  std::string encoded_folder_path;
  uint32_t num_repetitions = 0;  // 0 means encode/decode each image once,
                                 // 1 means encode/decode each image twice etc.
  uint32_t num_extra_threads = 0;  // 0 means single-threaded,
                                   // 1 and above means multi-threaded.
  bool random_order = false;  // If true, input paths are randomly permuted.
  bool discard_distortion_values = false;  // If true, recompute distortions.
  bool quiet = true;  // If true, avoids logging to stdout and stderr.
};

Status Compare(const std::vector<std::string>& image_paths,
               const ComparisonSettings& settings,
               const std::string& completed_tasks_file_path,
               const std::string& results_folder_path);

}  // namespace codec_compare_gen

#endif  // SRC_FRAMEWORK_H_
