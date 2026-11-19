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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

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

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)

class ScopedTempDir {
 public:
  ScopedTempDir()
      : path_(std::filesystem::path(::testing::TempDir()) /
              testing::UnitTest::GetInstance()->current_test_info()->name()) {
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
  }
  ~ScopedTempDir() { std::filesystem::remove_all(path_); }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void CreateFakeMetricBinary(const std::filesystem::path& binary_path,
                            const std::string& script_contents) {
  std::filesystem::create_directories(binary_path.parent_path());
  {
    std::ofstream file(binary_path, std::ios::binary);
    ASSERT_TRUE(file.is_open()) << "Failed to create " << binary_path;
    file << script_contents;
  }
  std::filesystem::permissions(binary_path, std::filesystem::perms::owner_all);
}

std::string ReadFileBytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

TEST(DistortionTest, BinaryMetricNoShellInjectionDssim) {
  const std::string png_path = std::string(data_path) + "gradient32x32.png";
  const StatusOr<Image> png =
      ReadStillImageOrAnimation(png_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(png.status, Status::kOk);

  ScopedTempDir temp_dir;
  const std::filesystem::path dssim_binary =
      temp_dir.path() / "_deps" / "dssim-src" / "target" / "release" / "dssim";
  CreateFakeMetricBinary(dssim_binary,
                         "#!/bin/sh\n"
                         "dir=$(dirname \"$0\")\n"
                         "printf '%s' \"$1\" > \"$dir/arg1\"\n"
                         "printf '%s' \"$2\" > \"$dir/arg2\"\n"
                         "printf '0.125\\timg\\n'\n");

  const std::filesystem::path sentinel = temp_dir.path() / "pwned";
  const std::string s = sentinel.string();
  const std::vector<std::string> payloads = {
      temp_dir.path().string() + "/a$(touch " + s + ").png",
      temp_dir.path().string() + "/a`touch " + s + "`.png",
      temp_dir.path().string() + "/a\"; touch " + s + "; #.png",
      temp_dir.path().string() + "/a\\\"; touch " + s + "; #.png",
      temp_dir.path().string() + "/a'$(touch " + s + ")'.png",
      temp_dir.path().string() + "/a\ntouch " + s + "\n.png",
      temp_dir.path().string() + "/a | touch " + s + ".png",
      temp_dir.path().string() + "/a\\.png",
  };

  for (const std::string& payload : payloads) {
    const StatusOr<float> distortion = GetAverageDistortion(
        payload, png.value, payload, png.value, {}, temp_dir.path().string(),
        DistortionMetric::kDssim, kThreadId, kQuiet);
    ASSERT_EQ(distortion.status, Status::kOk) << "Payload failed: " << payload;
    EXPECT_FLOAT_EQ(distortion.value, 0.125f);
    EXPECT_FALSE(std::filesystem::exists(sentinel))
        << "Shell injection triggered for payload: " << payload;
    EXPECT_EQ(ReadFileBytes(dssim_binary.parent_path() / "arg1"), payload);
    EXPECT_EQ(ReadFileBytes(dssim_binary.parent_path() / "arg2"), payload);
  }
}

TEST(DistortionTest, BinaryMetricNoShellInjectionLibjxl) {
  const std::string png_path = std::string(data_path) + "gradient32x32.png";
  const StatusOr<Image> png =
      ReadStillImageOrAnimation(png_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(png.status, Status::kOk);

  ScopedTempDir temp_dir;
  const std::filesystem::path sentinel = temp_dir.path() / "pwned";
  const std::filesystem::path binary_root =
      temp_dir.path() / ("bin$(touch " + sentinel.string() + ")");
  const std::filesystem::path butteraugli_binary =
      binary_root / "_deps" / "libjxl-build" / "tools" / "butteraugli_main";
  CreateFakeMetricBinary(butteraugli_binary,
                         "#!/bin/sh\n"
                         "dir=$(dirname \"$0\")\n"
                         "printf '%s' \"$1\" > \"$dir/arg1\"\n"
                         "printf '%s' \"$2\" > \"$dir/arg2\"\n"
                         "printf '1.5\\n3-norm: 0.75\\n'\n");

  const std::string malicious_path =
      temp_dir.path().string() + "/a$(touch " + sentinel.string() + ")\\.png";
  const StatusOr<float> butteraugli = GetAverageDistortion(
      malicious_path, png.value, malicious_path, png.value, {},
      binary_root.string(), DistortionMetric::kLibjxlButteraugli, kThreadId,
      kQuiet);
  ASSERT_EQ(butteraugli.status, Status::kOk);
  EXPECT_FLOAT_EQ(butteraugli.value, 1.5f);
  EXPECT_FALSE(std::filesystem::exists(sentinel));
  EXPECT_EQ(ReadFileBytes(butteraugli_binary.parent_path() / "arg1"),
            malicious_path);
  EXPECT_EQ(ReadFileBytes(butteraugli_binary.parent_path() / "arg2"),
            malicious_path);

  const StatusOr<float> p3norm = GetAverageDistortion(
      malicious_path, png.value, malicious_path, png.value, {},
      binary_root.string(), DistortionMetric::kLibjxlP3norm, kThreadId, kQuiet);
  ASSERT_EQ(p3norm.status, Status::kOk);
  EXPECT_FLOAT_EQ(p3norm.value, 0.75f);
  EXPECT_FALSE(std::filesystem::exists(sentinel));
}

TEST(DistortionTest, BinaryMetricLeadingDashPath) {
  const std::string png_path = std::string(data_path) + "gradient32x32.png";
  const StatusOr<Image> png =
      ReadStillImageOrAnimation(png_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(png.status, Status::kOk);

  ScopedTempDir temp_dir;
  const std::filesystem::path dssim_binary =
      temp_dir.path() / "_deps" / "dssim-src" / "target" / "release" / "dssim";
  CreateFakeMetricBinary(dssim_binary,
                         "#!/bin/sh\n"
                         "dir=$(dirname \"$0\")\n"
                         "printf '%s' \"$1\" > \"$dir/arg1\"\n"
                         "printf '%s' \"$2\" > \"$dir/arg2\"\n"
                         "printf '0.5\\timg\\n'\n");

  const StatusOr<float> distortion = GetAverageDistortion(
      "-ref.png", png.value, "--img.png", png.value, {},
      temp_dir.path().string(), DistortionMetric::kDssim, kThreadId, kQuiet);
  ASSERT_EQ(distortion.status, Status::kOk);
  EXPECT_FLOAT_EQ(distortion.value, 0.5f);
  EXPECT_EQ(ReadFileBytes(dssim_binary.parent_path() / "arg1"), "./-ref.png");
  EXPECT_EQ(ReadFileBytes(dssim_binary.parent_path() / "arg2"), "./--img.png");
}

TEST(DistortionTest, BinaryMetricErrors) {
  const std::string png_path = std::string(data_path) + "gradient32x32.png";
  const StatusOr<Image> png =
      ReadStillImageOrAnimation(png_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(png.status, Status::kOk);

  ScopedTempDir temp_dir;
  // Missing binary returns an error rather than throwing on empty stdout.
  EXPECT_EQ(GetAverageDistortion(png_path, png.value, png_path, png.value, {},
                                 temp_dir.path().string(),
                                 DistortionMetric::kDssim, kThreadId,
                                 /*quiet=*/true)
                .status,
            Status::kUnknownError);

  // Non-zero exit status returns an error.
  const std::filesystem::path dssim_binary =
      temp_dir.path() / "_deps" / "dssim-src" / "target" / "release" / "dssim";
  CreateFakeMetricBinary(dssim_binary,
                         "#!/bin/sh\nprintf '0.1\\timg\\n'\nexit 1\n");
  EXPECT_EQ(GetAverageDistortion(png_path, png.value, png_path, png.value, {},
                                 temp_dir.path().string(),
                                 DistortionMetric::kDssim, kThreadId,
                                 /*quiet=*/true)
                .status,
            Status::kUnknownError);

  // Embedded NUL byte in a path argument is rejected.
  CreateFakeMetricBinary(dssim_binary, "#!/bin/sh\nprintf '0.1\\timg\\n'\n");
  const std::string nul_path("foo\0bar.png", 11);
  EXPECT_EQ(GetAverageDistortion(nul_path, png.value, png_path, png.value, {},
                                 temp_dir.path().string(),
                                 DistortionMetric::kDssim, kThreadId,
                                 /*quiet=*/true)
                .status,
            Status::kUnknownError);
}

TEST(DistortionTest, BinaryMetricLargeOutputAndConcurrency) {
  const std::string png_path = std::string(data_path) + "gradient32x32.png";
  const StatusOr<Image> png =
      ReadStillImageOrAnimation(png_path.c_str(), WP2_ARGB_32, kQuiet);
  ASSERT_EQ(png.status, Status::kOk);

  ScopedTempDir temp_dir;
  const std::filesystem::path dssim_binary =
      temp_dir.path() / "_deps" / "dssim-src" / "target" / "release" / "dssim";
  CreateFakeMetricBinary(dssim_binary, "#!/bin/sh\nprintf '0.25\\t" +
                                           std::string(10000, 'x') + "\\n'\n");

  constexpr size_t kNumThreads = 8;
  constexpr size_t kIterations = 4;
  std::vector<std::thread> threads;
  std::vector<Status> statuses(kNumThreads, Status::kOk);
  threads.reserve(kNumThreads);
  for (size_t t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&, t]() {
      for (size_t i = 0; i < kIterations; ++i) {
        const StatusOr<float> distortion = GetAverageDistortion(
            png_path, png.value, png_path, png.value, {},
            temp_dir.path().string(), DistortionMetric::kDssim, t, kQuiet);
        if (distortion.status != Status::kOk || distortion.value != 0.25f) {
          statuses[t] = Status::kUnknownError;
          return;
        }
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  for (Status status : statuses) {
    EXPECT_EQ(status, Status::kOk);
  }
}

#endif  // !defined(_WIN32) && !defined(__EMSCRIPTEN__)

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
