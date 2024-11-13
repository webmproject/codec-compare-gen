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

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "src/base.h"
#include "src/framework.h"

namespace codec_compare_gen {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

class FrameworkTest : public testing::Test {
 protected:
  void SetUp() override {
    std::filesystem::remove_all(TempPath());
    std::filesystem::create_directory(TempPath());
  }
  void TearDown() override { std::filesystem::remove_all(TempPath()); }
  std::filesystem::path TempPath(const char* file_name = nullptr) {
    std::filesystem::path dir_path(::testing::TempDir());
    dir_path /= testing::UnitTest::GetInstance()->current_test_info()->name();
    if (file_name != nullptr) {
      dir_path /= file_name;
    }
    return dir_path;
  }
};

//------------------------------------------------------------------------------

TEST(TrivialFrameworkTest, Empty) {
  EXPECT_EQ(Compare({}, ComparisonSettings(), "", ""), Status::kUnknownError);
}

TEST_F(FrameworkTest, Simple) {
  ComparisonSettings settings;
  settings.codec_settings.push_back(
      {Codec::kWebp, Subsampling::kDefault, /*effort=*/0, kQualityLossless});
  EXPECT_EQ(Compare({std::string(data_path) + "gradient32x32.png"}, settings,
                    TempPath("completed_tasks.csv"), TempPath()),
            Status::kOk);
}

TEST_F(FrameworkTest, AllCodecsWithAlphaAndAnimation) {
  ComparisonSettings settings;
  settings.codec_settings.push_back(
      {Codec::kWebp, Subsampling::kDefault, /*effort=*/0, kQualityLossless});
  settings.codec_settings.push_back(
      {Codec::kWebp2, Subsampling::kDefault, /*effort=*/0, /*quality=*/0});
  settings.codec_settings.push_back(
      {Codec::kJpegXl, Subsampling::kDefault, /*effort=*/1, /*quality=*/50});
  settings.codec_settings.push_back(
      {Codec::kAvif, Subsampling::kDefault, /*effort=*/9, /*quality=*/10});
  settings.codec_settings.push_back({Codec::kCombination, Subsampling::kDefault,
                                     /*effort=*/0, /*quality=*/90});
  EXPECT_EQ(Compare({std::string(data_path) + "gradient32x32.png",
                     std::string(data_path) + "alpha1x17.png",
                     std::string(data_path) + "anim80x80.webp"},
                    settings, TempPath("completed_tasks.csv"), TempPath()),
            Status::kOk);
}

TEST_F(FrameworkTest, AllTraditionalCodecs) {
  ComparisonSettings settings;
  settings.codec_settings.push_back(
      {Codec::kWebp, Subsampling::k420, /*effort=*/6, /*quality=*/75});
  settings.codec_settings.push_back(
      {Codec::kJpegturbo, Subsampling::k420, /*effort=*/0, /*quality=*/90});
  settings.codec_settings.push_back(
      {Codec::kJpegli, Subsampling::k420, /*effort=*/0, /*quality=*/80});
  settings.codec_settings.push_back(
      {Codec::kJpegsimple, Subsampling::k420, /*effort=*/8, /*quality=*/70});
  // copybara:insert_begin(no mozjpeg in google3)
  // settings.codec_settings.push_back(
  //     {Codec::kJpegmoz, Subsampling::k420, /*effort=*/0, /*quality=*/60});
  // }
  // copybara:insert_end
  EXPECT_EQ(Compare({std::string(data_path) + "gradient32x32.png"}, settings,
                    TempPath("completed_tasks.csv"), TempPath()),
            Status::kOk);
}

TEST_F(FrameworkTest, AllChromaSubsamplings) {
  ComparisonSettings settings;
  settings.codec_settings.push_back(
      {Codec::kWebp2, Subsampling::k420, /*effort=*/0, /*quality=*/75});
  settings.codec_settings.push_back(
      {Codec::kWebp2, Subsampling::k444, /*effort=*/0, /*quality=*/75});
  settings.codec_settings.push_back(
      {Codec::kWebp2, Subsampling::kDefault, /*effort=*/0, /*quality=*/75});
  EXPECT_EQ(Compare({std::string(data_path) + "gradient32x32.png"}, settings,
                    TempPath("completed_tasks.csv"), TempPath()),
            Status::kOk);
}

TEST_F(FrameworkTest, ExperimentalCodecs) {
  ComparisonSettings settings;
  settings.codec_settings.push_back(
      {Codec::kSlimAvif, Subsampling::kDefault, /*effort=*/9, /*quality=*/75});
  EXPECT_EQ(Compare({std::string(data_path) + "gradient32x32.png",
                     std::string(data_path) + "alpha1x17.png"},
                    settings, TempPath("completed_tasks.csv"), TempPath()),
            Status::kOk);
}

//------------------------------------------------------------------------------

TEST_F(FrameworkTest, Incremental) {
  ComparisonSettings settings;
  settings.codec_settings.push_back(
      {Codec::kWebp, Subsampling::k444, /*effort=*/0, kQualityLossless});
  const std::vector<std::string> images = {
      std::string(data_path) + "alpha1x17.png",
      std::string(data_path) + "gradient32x32.png"};

  EXPECT_EQ(
      Compare(images, settings, TempPath("completed_tasks.csv"), TempPath()),
      Status::kOk);

  // Make sure the output files were created.
  ASSERT_EQ(std::filesystem::exists(TempPath("completed_tasks.csv")), true);
  const std::string expected_webp_results_file_path =
      TempPath("webp_444_0.json");
  ASSERT_EQ(std::filesystem::exists(expected_webp_results_file_path), true);
  const uintmax_t completed_tasks_file_size =
      std::filesystem::file_size(TempPath("completed_tasks.csv"));
  EXPECT_GT(completed_tasks_file_size, 0);
  const uintmax_t webp_results_file_size =
      std::filesystem::file_size(expected_webp_results_file_path);
  EXPECT_GT(webp_results_file_size, 0);

  // Call it again, with one more codec.
  settings.codec_settings.push_back(
      {Codec::kWebp2, Subsampling::k444, /*effort=*/0, kQualityLossless});
  EXPECT_EQ(
      Compare(images, settings, TempPath("completed_tasks.csv"), TempPath()),
      Status::kOk);

  // Output files increased in size.
  EXPECT_GT(std::filesystem::file_size(TempPath("completed_tasks.csv")),
            completed_tasks_file_size);
  // Only WebP2 files were added, the WebP one should be the same.
  EXPECT_EQ(std::filesystem::file_size(expected_webp_results_file_path),
            webp_results_file_size);

  // Call it again, with repetitions.
  settings.num_repetitions = 3;
  EXPECT_EQ(
      Compare(images, settings, TempPath("completed_tasks.csv"), TempPath()),
      Status::kOk);

  // It should be similar: more data was aggregated into the same result count.
  EXPECT_NEAR(std::filesystem::file_size(expected_webp_results_file_path),
              webp_results_file_size, webp_results_file_size / 10);

  // Call it again, noop.
  EXPECT_EQ(
      Compare(images, settings, TempPath("completed_tasks.csv"), TempPath()),
      Status::kOk);
}

//------------------------------------------------------------------------------

TEST_F(FrameworkTest, InconvenientFilePaths) {
  ComparisonSettings settings;
  settings.codec_settings = {
      {Codec::kWebp, Subsampling::k444, /*effort=*/0, kQualityLossless},
      {Codec::kWebp2, Subsampling::k444, /*effort=*/0, kQualityLossless}};
  const std::string image_alpha1x17_file_path = TempPath("al,pha1x17.\"png");
  std::filesystem::copy(std::string(data_path) + "alpha1x17.png",
                        image_alpha1x17_file_path);
  const std::string completed_tasks_file_path =
      TempPath("compl,eted_ta\"sks.csv");
  const std::string results_folder_path = TempPath("re\"s,ults");
  std::filesystem::create_directory(results_folder_path);

  EXPECT_EQ(Compare({image_alpha1x17_file_path}, settings,
                    completed_tasks_file_path, results_folder_path),
            Status::kOk);

  // Call it again, with one more image.
  const std::string image_gradient32x32_file_path = TempPath(",gr.die\"nt.png");
  std::filesystem::copy(std::string(data_path) + "gradient32x32.png",
                        image_gradient32x32_file_path);
  EXPECT_EQ(Compare({image_alpha1x17_file_path, image_gradient32x32_file_path},
                    settings, completed_tasks_file_path, results_folder_path),
            Status::kOk);
}

//------------------------------------------------------------------------------

TEST_F(FrameworkTest, DifferentImageSetOrCodecOrQuality) {
  ComparisonSettings settings;
  settings.quiet = false;
  settings.codec_settings = {
      {Codec::kWebp, Subsampling::k444, /*effort=*/0, kQualityLossless},
      {Codec::kWebp2, Subsampling::k444, /*effort=*/0, kQualityLossless}};

  EXPECT_EQ(Compare({std::string(data_path) + "alpha1x17.png",
                     std::string(data_path) + "gradient32x32.png"},
                    settings, TempPath("completed_tasks.csv"), TempPath()),
            Status::kOk);

  // Call it again, with a missing image.
  EXPECT_EQ(Compare({std::string(data_path) + "gradient32x32.png"}, settings,
                    TempPath("completed_tasks.csv"), TempPath()),
            Status::kUnknownError);

  // Call it again, with a missing codec.
  settings.codec_settings.pop_back();
  EXPECT_EQ(Compare({std::string(data_path) + "alpha1x17.png",
                     std::string(data_path) + "gradient32x32.png"},
                    settings, TempPath("completed_tasks.csv"), TempPath()),
            Status::kUnknownError);

  // Call it again, with a different quality.
  settings.codec_settings.push_back(
      {Codec::kWebp2, Subsampling::k444, /*effort=*/0, /*quality=*/100});
  EXPECT_EQ(Compare({std::string(data_path) + "alpha1x17.png",
                     std::string(data_path) + "gradient32x32.png"},
                    settings, TempPath("completed_tasks.csv"), TempPath()),
            Status::kUnknownError);

  // Call it again, with the missing element added back.
  settings.codec_settings.push_back(
      {Codec::kWebp2, Subsampling::k444, /*effort=*/0, kQualityLossless});
  EXPECT_EQ(Compare({std::string(data_path) + "alpha1x17.png",
                     std::string(data_path) + "gradient32x32.png"},
                    settings, TempPath("completed_tasks.csv"), TempPath()),
            Status::kOk);
}

//------------------------------------------------------------------------------

}  // namespace
}  // namespace codec_compare_gen

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "There must be exactly one argument containing the path to "
                 "the test data folder"
              << std::endl;
    return 1;
  }
  codec_compare_gen::data_path = argv[1];
  return RUN_ALL_TESTS();
}
