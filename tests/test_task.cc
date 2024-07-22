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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "src/base.h"
#include "src/task.h"

namespace codec_compare_gen {
namespace {

void ExpectEq(const std::vector<std::vector<TaskOutput>>& actual,
              const std::vector<std::vector<TaskOutput>>& expected) {
  ASSERT_EQ(actual.size(), expected.size());
  for (int i = 0; i < actual.size(); ++i) {
    ASSERT_EQ(actual[i].size(), expected[i].size());
    for (int j = 0; j < actual[i].size(); ++j) {
      EXPECT_EQ(actual[i][j].task_input, expected[i][j].task_input);
      EXPECT_EQ(actual[i][j].image_width, expected[i][j].image_width);
      EXPECT_EQ(actual[i][j].image_height, expected[i][j].image_height);
      EXPECT_EQ(actual[i][j].encoded_size, expected[i][j].encoded_size);
      EXPECT_EQ(actual[i][j].encoding_duration,
                expected[i][j].encoding_duration);
      EXPECT_EQ(actual[i][j].decoding_duration,
                expected[i][j].decoding_duration);
      EXPECT_EQ(actual[i][j].decoding_color_conversion_duration,
                expected[i][j].decoding_color_conversion_duration);
      for (size_t m = 0; m < kNumDistortionMetrics; ++m) {
        EXPECT_EQ(actual[i][j].distortions[m], expected[i][j].distortions[m]);
      }
    }
  }
}

constexpr Codec kWebp = Codec::kWebp;
constexpr Codec kWebp2 = Codec::kWebp2;
constexpr Subsampling kDef = Subsampling::kDefault;

TEST(SplitByCodecSettingsAndAggregateByImageTest, Simple) {
  const std::vector<TaskOutput> results = {
      {{{kWebp, kDef, /*effort=*/0, /*quality=*/0}, "img"}, 1, 2, 3, 0}};
  const auto aggregate = SplitByCodecSettingsAndAggregateByImageAndQuality(
      results, /*quiet=*/false);
  ASSERT_EQ(aggregate.status, Status::kOk);
  ExpectEq(aggregate.value, {results});
}

TEST(SplitByCodecSettingsAndAggregateByImageTest, Multiple) {
  const std::vector<TaskInput> single_inputs = {
      {{kWebp, kDef, /*effort=*/0, /*quality=*/0}, "imgA"},
      {{kWebp, kDef, /*effort=*/0, /*quality=*/0}, "imgB"},
      {{kWebp, kDef, /*effort=*/1, /*quality=*/0}, "imgA"},
      {{kWebp, kDef, /*effort=*/0, /*quality=*/100}, "imgA"},
      {{kWebp2, kDef, /*effort=*/0, /*quality=*/0}, "imgA"}};
  std::vector<TaskOutput> results;
  results.reserve(single_inputs.size() * 2);
  constexpr uint32_t kImageWidth = 8;
  constexpr uint32_t kImageHeight = 9;
  size_t encoded_size = 1;
  double encoding_duration = 1;
  double decoding_duration = 1;
  double decoding_color_conversion_duration = 0;
  float distortion = 20;
  for (const TaskInput& input : single_inputs) {
    // Simulate repetitions. Repetitions exist for more stable timings. Size and
    // distortion metrics do not vary.
    results.push_back({input,
                       kImageWidth,
                       kImageHeight,
                       encoded_size,
                       encoding_duration++,
                       decoding_duration++,
                       decoding_color_conversion_duration++,
                       {distortion}});
    results.push_back({input,
                       kImageWidth,
                       kImageHeight,
                       encoded_size++,
                       encoding_duration++,
                       decoding_duration++,
                       decoding_color_conversion_duration,
                       {distortion}});
    distortion += 1;
  }
  std::random_device rd;
  std::shuffle(results.begin(), results.end(), std::mt19937(rd()));

  const auto aggregate = SplitByCodecSettingsAndAggregateByImageAndQuality(
      results, /*quiet=*/false);
  ASSERT_EQ(aggregate.status, Status::kOk);
  ExpectEq(
      aggregate.value,
      {{{{{kWebp, kDef, 0, 0}, "imgA"}, 8, 9, 1u, 1.5, 1.5, 0.5, {20.0}},
        {{{kWebp, kDef, 0, 100}, "imgA"}, 8, 9, 4u, 7.5, 7.5, 3.5, {23.0}},
        {{{kWebp, kDef, 0, 0}, "imgB"}, 8, 9, 2u, 3.5, 3.5, 1.5, {21.0}}},
       {{{{kWebp, kDef, 1, 0}, "imgA"}, 8, 9, 3u, 5.5, 5.5, 2.5, {22.0}}},
       {{{{kWebp2, kDef, 0, 0}, "imgA"}, 8, 9, 5u, 9.5, 9.5, 4.5, {24.0}}}});
}

}  // namespace
}  // namespace codec_compare_gen
