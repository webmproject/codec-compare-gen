// Copyright 2026 Google LLC
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

#include <iostream>
#include <string>

#include "gtest/gtest.h"
#include "src/base.h"
#include "src/codec.h"
#include "src/task.h"

namespace codec_compare_gen {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

//------------------------------------------------------------------------------

TEST(WebpRsTest, Lossless) {
  const std::string image_path = std::string(data_path) + "gradient32x32.png";
  TaskInput input = {{Codec::kWebpRs, Subsampling::k444, /*effort=*/0,
                      /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  ASSERT_EQ(result.status, Status::kOk);
  EXPECT_GT(result.value.encoded_size, 0);
}

TEST(WebpRsTest, LosslessAlpha) {
  const std::string image_path = std::string(data_path) + "alpha1x17.png";
  TaskInput input = {{Codec::kWebpRs, Subsampling::k444, /*effort=*/0,
                      /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  ASSERT_EQ(result.status, Status::kOk);
  EXPECT_GT(result.value.encoded_size, 0);
}

TEST(WebpRsTest, LossyFails) {
  const std::string image_path = std::string(data_path) + "gradient32x32.png";
  TaskInput input = {
      {Codec::kWebpRs, Subsampling::k420, /*effort=*/0, /*quality=*/75},
      image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/true);
  EXPECT_NE(result.status, Status::kOk);
}

TEST(WebpRsTest, AnimationFails) {
  const std::string image_path = std::string(data_path) + "anim80x80.gif";
  TaskInput input = {{Codec::kWebpRs, Subsampling::k444, /*effort=*/0,
                      /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/true);
  EXPECT_NE(result.status, Status::kOk);
}

//------------------------------------------------------------------------------
// libwebp as the encoder and the image-webp crate as the decoder.

TEST(WebpEncWebpRsDecTest, Lossless) {
  const std::string image_path = std::string(data_path) + "gradient32x32.png";
  TaskInput input = {{Codec::kWebpEncWebpRsDec, Subsampling::k444, /*effort=*/0,
                      /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  ASSERT_EQ(result.status, Status::kOk);
  EXPECT_GT(result.value.encoded_size, 0);
}

TEST(WebpEncWebpRsDecTest, LosslessAlpha) {
  const std::string image_path = std::string(data_path) + "alpha1x17.png";
  TaskInput input = {{Codec::kWebpEncWebpRsDec, Subsampling::k444, /*effort=*/0,
                      /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  ASSERT_EQ(result.status, Status::kOk);
  EXPECT_GT(result.value.encoded_size, 0);
}

TEST(WebpEncWebpRsDecTest, Lossy) {
  const std::string image_path = std::string(data_path) + "gradient32x32.png";
  TaskInput input = {{Codec::kWebpEncWebpRsDec, Subsampling::k420, /*effort=*/0,
                      /*quality=*/75},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/true);
  ASSERT_EQ(result.status, Status::kOk);
  EXPECT_GT(result.value.encoded_size, 0);
  EXPECT_GT(result.value
                .distortions[static_cast<int>(DistortionMetric::kLibwebp2Psnr)],
            25.0f);
}

TEST(WebpEncWebpRsDecTest, LosslessAnimation) {
  const std::string image_path = std::string(data_path) + "anim80x80.gif";
  TaskInput input = {{Codec::kWebpEncWebpRsDec, Subsampling::k444, /*effort=*/0,
                      /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  // TODO(b/509475659): The output of the image-webp crate does not perfectly
  //                    matches the output of libwebp as of version 0.2.4.
  //                    codec-compare-gen fails because enc/dec is not lossless.
  ASSERT_NE(result.status, Status::kOk);
}

TEST(WebpEncWebpRsDecTest, LossyAnimation) {
  const std::string image_path = std::string(data_path) + "anim80x80.gif";
  TaskInput input = {{Codec::kWebpEncWebpRsDec, Subsampling::k420, /*effort=*/0,
                      /*quality=*/75},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  ASSERT_EQ(result.status, Status::kOk);
  EXPECT_GT(result.value.encoded_size, 0);
  EXPECT_GT(result.value
                .distortions[static_cast<int>(DistortionMetric::kLibwebp2Psnr)],
            // TODO(b/509475659): Expect 25dB once the fix is released in a
            //                    tagged version. See
            //                    https://github.com/image-rs/image/issues/2913.
            24.0f);
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
