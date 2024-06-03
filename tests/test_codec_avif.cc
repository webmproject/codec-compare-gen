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

#include <numeric>
#include <vector>

#include "gtest/gtest.h"
#include "src/base.h"
#include "src/codec.h"

namespace codec_compare_gen {
namespace {

// Taken from avifQualityToQuantizer() at
// https://github.com/AOMediaCodec/libavif/blob/4865c1c/src/write.c#L1097-L1110
static int QualityToQuantizer(int quality) {
  return ((100 - quality) * 63 + 50) / 100;
}

TEST(AvifTest, Qualities) {
  const std::vector<int> qualities = CodecLossyQualities(Codec::kAvif);
  std::vector<int> quantizers;
  quantizers.reserve(qualities.size());
  for (int quality : qualities) {
    quantizers.push_back(QualityToQuantizer(quality));
  }

  std::vector<int> expected_quantizers(64);
  std::iota(expected_quantizers.begin(), expected_quantizers.end(), 0);

  // Make sure the AVIF quality list maps to the exact range [0:63] without gaps
  // or duplicates.
  EXPECT_EQ(quantizers, expected_quantizers);
}

}  // namespace
}  // namespace codec_compare_gen
