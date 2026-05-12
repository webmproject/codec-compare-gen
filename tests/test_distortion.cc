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

#include <cstddef>
#include <iostream>
#include <string>

#include "gtest/gtest.h"
#include "src/base.h"
#include "src/distortion.h"
#include "src/frame.h"
#include "src/wp2/base.h"

namespace codec_compare_gen {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

// Used to pass the metric binary folder path to the GoogleTest suites.
const char* binary_path = nullptr;

constexpr bool kQuiet = false;
constexpr size_t kThreadId = 0;

//------------------------------------------------------------------------------

TEST(DistortionTest, Same) {
  const std::string gif_path = std::string(data_path) + "anim80x80.gif";
  const StatusOr<Image> gif =
      ReadStillImageOrAnimation(gif_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(gif.status, Status::kOk);

  const StatusOr<bool> equality = PixelEquality(gif.value, gif.value, kQuiet);
  ASSERT_EQ(equality.status, Status::kOk);
  EXPECT_TRUE(equality.value);

  const StatusOr<float> distortion =
      GetAverageDistortion("", gif.value, "", gif.value, {}, "",
                           DistortionMetric::kLibwebp2Psnr, kThreadId, kQuiet);
  ASSERT_EQ(distortion.status, Status::kOk);
  EXPECT_EQ(distortion.value, kNoDistortion);
}

TEST(DistortionTest, DifferentPixels) {
  const std::string gif_path = std::string(data_path) + "anim80x80.gif";
  const std::string webp_path = std::string(data_path) + "anim80x80.webp";
  const StatusOr<Image> gif =
      ReadStillImageOrAnimation(gif_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(gif.status, Status::kOk);
  const StatusOr<Image> webp =
      ReadStillImageOrAnimation(webp_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(webp.status, Status::kOk);

  const StatusOr<bool> equality = PixelEquality(gif.value, webp.value, kQuiet);
  ASSERT_EQ(equality.status, Status::kOk);
  EXPECT_FALSE(equality.value);

  const StatusOr<float> distortion =
      GetAverageDistortion("", gif.value, "", webp.value, {}, "",
                           DistortionMetric::kLibwebp2Psnr, kThreadId, kQuiet);
  ASSERT_EQ(distortion.status, Status::kOk);
  // Expect a distortion equivalent to GIF not supporting translucency.
  EXPECT_LT(distortion.value, kNoDistortion);
  EXPECT_GT(distortion.value, 20.0f);
}

TEST(DistortionTest, DifferentDuration) {
  const std::string path = std::string(data_path) + "anim80x80.gif";
  const StatusOr<Image> animation =
      ReadStillImageOrAnimation(path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(animation.status, Status::kOk);
  StatusOr<Image> shorter =
      ReadStillImageOrAnimation(path.c_str(), WP2_ARGB_32, kQuiet);

  ASSERT_EQ(shorter.status, Status::kOk);
  shorter.value.front().duration_ms /= 2;

  EXPECT_EQ(PixelEquality(animation.value, shorter.value, kQuiet).status,
            Status::kUnknownError);
  EXPECT_EQ(
      GetAverageDistortion("", animation.value, "", shorter.value, {}, "",
                           DistortionMetric::kLibwebp2Psnr, kThreadId, kQuiet)
          .status,
      Status::kUnknownError);
}

TEST(DistortionTest, DifferentFrameCount) {
  const std::string gif_path = std::string(data_path) + "anim80x80.gif";
  const std::string webp_path = std::string(data_path) + "anim80x80.webp";
  const StatusOr<Image> gif =
      ReadStillImageOrAnimation(gif_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(gif.status, Status::kOk);
  StatusOr<Image> webp =
      ReadStillImageOrAnimation(webp_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(webp.status, Status::kOk);

  // Copy the first frame and append it to the end, but keep the same duration.
  webp.value.back().duration_ms -= 3;
  webp.value.emplace_back(WP2::ArgbBuffer(WP2_ARGB_32), 3);
  ASSERT_EQ(webp.value.back().pixels.SetView(webp.value.front().pixels),
            WP2_STATUS_OK);

  const StatusOr<bool> equality = PixelEquality(gif.value, webp.value, kQuiet);
  ASSERT_EQ(equality.status, Status::kOk);
  EXPECT_FALSE(equality.value);

  const StatusOr<float> distortion =
      GetAverageDistortion("", gif.value, "", webp.value, {}, "",
                           DistortionMetric::kLibwebp2Psnr, kThreadId, kQuiet);
  ASSERT_EQ(distortion.status, Status::kOk);
  // Expect a distortion equivalent to equality but last frame.
  EXPECT_LT(distortion.value, 25.0f);
  EXPECT_GT(distortion.value, 20.0f);
}

TEST(DistortionTest, Dssim) {
  const std::string gif_path = std::string(data_path) + "anim80x80.gif";
  const StatusOr<Image> gif =
      ReadStillImageOrAnimation(gif_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(gif.status, Status::kOk);

  const StatusOr<float> distortion = GetAverageDistortion(
      gif_path, gif.value, gif_path, gif.value, {}, binary_path,
      DistortionMetric::kDssim, kThreadId, kQuiet);
  ASSERT_EQ(distortion.status, Status::kOk);
  EXPECT_EQ(distortion.value, 0);  // Equality.
}

TEST(DistortionTest, Ssimulacra) {
  const std::string gif_path = std::string(data_path) + "anim80x80.gif";
  const StatusOr<Image> gif =
      ReadStillImageOrAnimation(gif_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(gif.status, Status::kOk);

  const StatusOr<float> distortion = GetAverageDistortion(
      gif_path, gif.value, gif_path, gif.value, {}, binary_path,
      DistortionMetric::kLibjxlSsimulacra, kThreadId, kQuiet);
  ASSERT_EQ(distortion.status, Status::kOk);
  EXPECT_EQ(distortion.value, 0);  // Equality.
}

TEST(DistortionTest, Ssimulacra2) {
  const std::string gif_path = std::string(data_path) + "anim80x80.gif";
  const StatusOr<Image> gif =
      ReadStillImageOrAnimation(gif_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(gif.status, Status::kOk);

  const StatusOr<float> distortion = GetAverageDistortion(
      gif_path, gif.value, gif_path, gif.value, {}, binary_path,
      DistortionMetric::kLibjxlSsimulacra2, kThreadId, kQuiet);
  ASSERT_EQ(distortion.status, Status::kOk);
  EXPECT_EQ(distortion.value, 100);  // Equality.
}

TEST(DistortionTest, Butteraugli) {
  const std::string gif_path = std::string(data_path) + "anim80x80.gif";
  const StatusOr<Image> gif =
      ReadStillImageOrAnimation(gif_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(gif.status, Status::kOk);

  const StatusOr<float> distortion = GetAverageDistortion(
      gif_path, gif.value, gif_path, gif.value, {}, binary_path,
      DistortionMetric::kLibjxlButteraugli, kThreadId, kQuiet);
  ASSERT_EQ(distortion.status, Status::kOk);
  EXPECT_EQ(distortion.value, 0);  // Equality.
}

TEST(DistortionTest, P3norm) {
  const std::string gif_path = std::string(data_path) + "anim80x80.gif";
  const StatusOr<Image> gif =
      ReadStillImageOrAnimation(gif_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(gif.status, Status::kOk);

  const StatusOr<float> distortion = GetAverageDistortion(
      gif_path, gif.value, gif_path, gif.value, {}, binary_path,
      DistortionMetric::kLibjxlP3norm, kThreadId, kQuiet);
  ASSERT_EQ(distortion.status, Status::kOk);
  EXPECT_EQ(distortion.value, 0);  // Equality.
}

//------------------------------------------------------------------------------

}  // namespace
}  // namespace codec_compare_gen

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 3) {
    std::cerr << "There must be exactly two arguments containing the paths to "
                 "the test data folder and to the binary folder."
              << std::endl;
    return 1;
  }
  codec_compare_gen::data_path = argv[1];
  codec_compare_gen::binary_path = argv[2];
  return RUN_ALL_TESTS();
}
