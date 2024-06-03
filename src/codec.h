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

#ifndef SRC_CODEC_H_
#define SRC_CODEC_H_

#include <cstddef>
#include <string>
#include <vector>

#include "src/base.h"
#include "src/task.h"

namespace codec_compare_gen {

std::string CodecName(Codec codec);
std::string CodecVersion(Codec codec);
StatusOr<Codec> CodecFromName(const std::string& name, bool quiet);
std::vector<int> CodecLossyQualities(Codec codec);
std::string CodecExtension(Codec codec);
bool CodecIsSupportedByBrowsers(Codec codec);

enum class EncodeMode { kEncode, kEncodeAndSaveToDisk, kLoadFromDisk };

StatusOr<TaskOutput> EncodeDecode(const TaskInput& input,
                                  const std::string& metric_binary_folder_path,
                                  size_t thread_id, EncodeMode encode_mode,
                                  bool quiet);

}  // namespace codec_compare_gen

#endif  // SRC_CODEC_H_
