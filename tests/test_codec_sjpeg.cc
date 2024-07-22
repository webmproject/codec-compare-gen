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

TEST(SjpegTest, Subsamplings) {
  const std::string image_path = std::string(data_path) + "gradient32x32.png";
  TaskInput input = {
      {Codec::kJpegsimple, Subsampling::k444, /*effort=*/0, /*quality=*/75},
      image_path};

  const StatusOr<TaskOutput> result444 =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  ASSERT_EQ(result444.status, Status::kOk);

  input.codec_settings.chroma_subsampling = Subsampling::k420;
  const StatusOr<TaskOutput> result420 =
      EncodeDecode(input, "", 0, EncodeMode::kEncode, /*quiet=*/false);
  ASSERT_EQ(result420.status, Status::kOk);

  EXPECT_GT(result444.value.encoded_size, result420.value.encoded_size);
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
