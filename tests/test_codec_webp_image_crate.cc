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

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "src/base.h"
#include "src/codec.h"
#include "src/frame.h"
#include "src/task.h"
#include "src/wp2/base.h"

namespace codec_compare_gen {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

//------------------------------------------------------------------------------

TEST(WebpImageCrateTest, Lossless) {
  const std::string image_path = std::string(data_path) + "gradient32x32.png";
  TaskInput input = {{Codec::kWebpImageCrate, Subsampling::k444, /*effort=*/0,
                      /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  ASSERT_EQ(result.status, Status::kOk);
  EXPECT_GT(result.value.encoded_size, 0);
}

TEST(WebpImageCrateTest, LosslessAlpha) {
  const std::string image_path = std::string(data_path) + "alpha1x17.png";
  TaskInput input = {{Codec::kWebpImageCrate, Subsampling::k444, /*effort=*/0,
                      /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  ASSERT_EQ(result.status, Status::kOk);
  EXPECT_GT(result.value.encoded_size, 0);
}

TEST(WebpImageCrateTest, LossyFails) {
  const std::string image_path = std::string(data_path) + "gradient32x32.png";
  TaskInput input = {{Codec::kWebpImageCrate, Subsampling::k420, /*effort=*/0,
                      /*quality=*/75},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/true);
  EXPECT_NE(result.status, Status::kOk);
}

TEST(WebpImageCrateTest, AnimationFails) {
  const std::string image_path = std::string(data_path) + "anim80x80.gif";
  TaskInput input = {{Codec::kWebpImageCrate, Subsampling::k444, /*effort=*/0,
                      /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/true);
  EXPECT_NE(result.status, Status::kOk);
}

//------------------------------------------------------------------------------
// libwebp as the encoder and the image-webp crate as the decoder.

TEST(WebpEncWebpImageCrateDecTest, Lossless) {
  const std::string image_path = std::string(data_path) + "gradient32x32.png";
  TaskInput input = {{Codec::kWebpEncWebpImageCrateDec, Subsampling::k444,
                      /*effort=*/0, /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  ASSERT_EQ(result.status, Status::kOk);
  EXPECT_GT(result.value.encoded_size, 0);
}

TEST(WebpEncWebpImageCrateDecTest, LosslessAlpha) {
  const std::string image_path = std::string(data_path) + "alpha1x17.png";
  TaskInput input = {{Codec::kWebpEncWebpImageCrateDec, Subsampling::k444,
                      /*effort=*/0, /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  ASSERT_EQ(result.status, Status::kOk);
  EXPECT_GT(result.value.encoded_size, 0);
}

TEST(WebpEncWebpImageCrateDecTest, Lossy) {
  const std::string image_path = std::string(data_path) + "gradient32x32.png";
  TaskInput input = {{Codec::kWebpEncWebpImageCrateDec, Subsampling::k420,
                      /*effort=*/0, /*quality=*/75},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/true);
  ASSERT_EQ(result.status, Status::kOk);
  EXPECT_GT(result.value.encoded_size, 0);
  EXPECT_GT(result.value
                .distortions[static_cast<int>(DistortionMetric::kLibwebp2Psnr)],
            25.0f);
}

TEST(WebpEncWebpImageCrateDecTest, LosslessAnimation) {
  const std::string image_path = std::string(data_path) + "anim80x80.gif";
  TaskInput input = {{Codec::kWebpEncWebpImageCrateDec, Subsampling::k444,
                      /*effort=*/0, /*quality=*/kQualityLossless},
                     image_path};

  const StatusOr<TaskOutput> result =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  // TODO(b/509475659): The output of the image-webp crate does not perfectly
  //                    matches the output of libwebp as of version 0.2.4.
  //                    codec-compare-gen fails because enc/dec is not lossless.
  ASSERT_NE(result.status, Status::kOk);
}

TEST(WebpEncWebpImageCrateDecTest, LossyAnimation) {
  const std::string image_path = std::string(data_path) + "anim80x80.gif";
  TaskInput input = {{Codec::kWebpEncWebpImageCrateDec, Subsampling::k420,
                      /*effort=*/0, /*quality=*/75},
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

TEST(WebpImageCrateTest, AnimatedTimestampOverflow) {
  const std::string image_path =
      std::string(data_path) + "anim1x1_timestamp_overflow.webp";
  std::ifstream file(image_path, std::ios::in | std::ios::binary);
  ASSERT_TRUE(file.good());
  const std::vector<uint8_t> webp((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
  WP2::Data encoded;
  ASSERT_EQ(encoded.CopyFrom(webp.data(), webp.size()), WP2_STATUS_OK);

  const TaskInput input = {{Codec::kWebpImageCrate, Subsampling::k444,
                            /*effort=*/0, /*quality=*/kQualityLossless},
                           image_path};
  const StatusOr<std::pair<Image, double>> decoded =
      GetCodecMetadata(Codec::kWebpImageCrate)
          .decode(input, encoded, /*quiet=*/false);
  ASSERT_EQ(decoded.status, Status::kOk);
  constexpr uint32_t kNumFrames = 130;
  constexpr uint32_t kMaxDurationMs = 0xffffff;
  ASSERT_EQ(decoded.value.first.size(), kNumFrames);
  EXPECT_EQ(decoded.value.first[0].duration_ms, kMaxDurationMs);
  EXPECT_EQ(decoded.value.first[127].duration_ms, kMaxDurationMs);
  // Frame 129 saturates the cumulative timestamp to INT_MAX.
  EXPECT_EQ(decoded.value.first[128].duration_ms,
            static_cast<uint32_t>(std::numeric_limits<int>::max()) -
                128u * kMaxDurationMs);
  EXPECT_EQ(decoded.value.first[129].duration_ms, 0u);
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
