// Copyright 2025 Google LLC
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

#ifndef SRC_CODEC_BASIS_H_
#define SRC_CODEC_BASIS_H_

#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/frame.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

namespace codec_compare_gen {

std::string BasisVersion();

class BasisContext {
 public:
  BasisContext(bool enabled);
  ~BasisContext();

 private:
  bool enabled;
};

std::vector<int> BasisLossyQualities();

#if defined(HAS_WEBP2)
StatusOr<WP2::Data> EncodeBasis(const TaskInput& input,
                                const Image& original_image, bool quiet);
StatusOr<std::pair<Image, double>> DecodeBasis(const TaskInput& input,
                                               const WP2::Data& encoded_image,
                                               bool quiet);
#endif  // HAS_WEBP2

}  // namespace codec_compare_gen

#endif  // SRC_CODEC_BASIS_H_
