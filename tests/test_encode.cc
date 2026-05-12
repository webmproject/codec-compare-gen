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
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "src/base.h"
#include "src/codec.h"

namespace codec_compare_gen {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

Status EncodeTest(const std::string& image_path, bool quiet = false) {
  std::ifstream file(image_path, std::ios::in | std::ios::binary);
  CHECK_OR_RETURN(file.good(), quiet);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
  uint32_t width, height;
  ASSIGN_OR_RETURN(
      std::vector<uint8_t> argb,
      DecodeToArgb(bytes.data(), bytes.size(), &width, &height, quiet));
  ASSIGN_OR_RETURN(std::vector<uint8_t> encoded_image,
                   Encode(argb.data(), width, height, Codec::kWebp,
                          Subsampling::kDefault, 0, kQualityLossless, quiet));
  uint32_t decoded_width, decoded_height;
  ASSIGN_OR_RETURN(std::vector<uint8_t> decoded_argb,
                   DecodeToArgb(encoded_image.data(), encoded_image.size(),
                                &decoded_width, &decoded_height, quiet));
  CHECK_OR_RETURN(width == decoded_width && height == decoded_height, quiet);
  CHECK_OR_RETURN(argb.size() == decoded_argb.size(), quiet);
  CHECK_OR_RETURN(argb == decoded_argb, quiet);
  return Status::kOk;
}

//------------------------------------------------------------------------------

TEST(EncodeTest, WrongPath) {
  EXPECT_EQ(EncodeTest("wrong path", /*quiet=*/true), Status::kUnknownError);
}

TEST(EncodeTest, WebPMinEffort) {
  EXPECT_EQ(EncodeTest(std::string(data_path) + "gradient32x32.png"),
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
