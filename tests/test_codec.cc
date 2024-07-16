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

#include <filesystem>
#include <iostream>
#include <string>

#include "gtest/gtest.h"
#include "src/base.h"
#include "src/codec.h"
#include "src/framework.h"
#include "src/task.h"

namespace codec_compare_gen {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

Status EncodeDecodeTest(const TaskInput& input, bool quiet = false) {
  return EncodeDecode(input, /*metric_binary_folder_path=*/"", /*thread_id=*/0,
                      EncodeMode::kEncode, quiet)
      .status;
}

//------------------------------------------------------------------------------

TEST(CodecTest, Empty) {
  EXPECT_EQ(EncodeDecodeTest(TaskInput(), /*quiet=*/true),
            Status::kUnknownError);
}

//------------------------------------------------------------------------------

TEST(CodecTest, WebPMinEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kWebp, /*effort=*/0, kQualityLossless};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, WebPMaxEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kWebp, /*effort=*/9, kQualityLossless};
  input.image_path = std::string(data_path) + "alpha1x17.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, WebPLossyMinQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kWebp, /*effort=*/5, /*quality=*/0};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, WebPLossyMaxQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kWebp, /*effort=*/5, /*quality=*/100};
  input.image_path = std::string(data_path) + "alpha1x17.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, WebPWrongEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kWebp, /*effort=*/10, kQualityLossless};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input, /*quiet=*/true), Status::kUnknownError);
}

TEST(CodecTest, WebPWrongQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kWebp, /*effort=*/5, 123};
  input.image_path = std::string(data_path) + "alpha1x17.png";
  EXPECT_EQ(EncodeDecodeTest(input, /*quiet=*/true), Status::kUnknownError);
}

//------------------------------------------------------------------------------

TEST(CodecTest, WebP2MinEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kWebp2, /*effort=*/0, kQualityLossless};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, WebP2MaxEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kWebp2, /*effort=*/9, kQualityLossless};
  input.image_path = std::string(data_path) + "alpha1x17.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, Web2PLossyMinQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kWebp2, /*effort=*/5, /*quality=*/0};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, Web2PLossyMaxQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kWebp2, /*effort=*/5, /*quality=*/95};
  input.image_path = std::string(data_path) + "alpha1x17.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

//------------------------------------------------------------------------------

TEST(CodecTest, JpegXlMinEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kJpegXl, /*effort=*/1, kQualityLossless};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, JpegXlMaxEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kJpegXl, /*effort=*/9, kQualityLossless};
  input.image_path = std::string(data_path) + "alpha1x17.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, JpegXlLossyMinQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kJpegXl, /*effort=*/5, /*quality=*/5};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, JpegXlLossyMaxQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kJpegXl, /*effort=*/5, /*quality=*/99};
  input.image_path = std::string(data_path) + "alpha1x17.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

//------------------------------------------------------------------------------

TEST(CodecTest, AvifMinEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kAvif, /*effort=*/9};  // max speed
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, AvifMaxEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kAvif, /*effort=*/0};  // min speed
  input.image_path = std::string(data_path) + "alpha1x17.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

//------------------------------------------------------------------------------

TEST(CodecTest, CodecCombinationMinEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kCombination, /*effort=*/0, kQualityLossless};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, CodecCombinationMediumEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kCombination, /*effort=*/5, kQualityLossless};
  input.image_path = std::string(data_path) + "alpha1x17.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, CodecCombinationMaxEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kCombination, /*effort=*/9, kQualityLossless};
  input.image_path = std::string(data_path) + "alpha1x17.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, CodecCombinationLossyMinQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kCombination, /*effort=*/5, /*quality=*/5};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, CodecCombinationLossyMaxQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kCombination, /*effort=*/5, /*quality=*/95};
  input.image_path = std::string(data_path) + "alpha1x17.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

//------------------------------------------------------------------------------

TEST(CodecTest, JpegturboMinQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kJpegturbo, /*effort=*/0, /*quality=*/0};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, JpegturboMaxQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kJpegturbo, /*effort=*/0, /*quality=*/100};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

//------------------------------------------------------------------------------

TEST(CodecTest, JpegliMinQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kJpegli, /*effort=*/0, /*quality=*/0};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, JpegliMaxQuality) {
  TaskInput input;
  input.codec_settings = {Codec::kJpegli, /*effort=*/0, /*quality=*/100};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

//------------------------------------------------------------------------------

TEST(CodecTest, JpegsimpleMinQualityMinEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kJpegsimple, /*effort=*/0, /*quality=*/0};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

TEST(CodecTest, JpegsimpleMaxQualityMaxEffort) {
  TaskInput input;
  input.codec_settings = {Codec::kJpegsimple, /*effort=*/8, /*quality=*/100};
  input.image_path = std::string(data_path) + "gradient32x32.png";
  EXPECT_EQ(EncodeDecodeTest(input), Status::kOk);
}

//------------------------------------------------------------------------------

TEST(CodecTest, EncodeToDiskAndLoadFromDisk) {
  TaskInput input;
  input.codec_settings = {Codec::kWebp, /*effort=*/2, /*quality=*/95};
  input.image_path = std::string(data_path) + "alpha1x17.png";
  input.encoded_path =
      std::filesystem::path(::testing::TempDir()) / "alpha1x17_webp_e2q95.webp";
  EXPECT_EQ(EncodeDecode(input, "", 0, EncodeMode::kEncodeAndSaveToDisk, false)
                .status,
            Status::kOk);
  EXPECT_EQ(EncodeDecode(input, "", 0, EncodeMode::kLoadFromDisk, false).status,
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
