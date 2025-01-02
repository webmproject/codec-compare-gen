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
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "tools/ccgen_impl.h"

namespace codec_compare_gen {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

//------------------------------------------------------------------------------

template <class... Args>
int TestMain(Args... args) {
  const char* const argv[] = {"ccgen", args...};
  return Main(sizeof(argv) / sizeof(argv[0]), argv);
}

TEST(CodecCompareGenTest, Help) {
  EXPECT_EQ(TestMain("-h"), 0);
  EXPECT_EQ(TestMain("--help"), 0);
}

TEST(CodecCompareGenTest, Run) {
  const std::string file_path = std::string(data_path) + "gradient32x32.png";
  const char* const path = file_path.c_str();
  EXPECT_EQ(TestMain(path, "--lossless", "--codec", "webp", "444", "6"), 0);
  EXPECT_EQ(TestMain(path, "--lossy", "--codec", "webp", "420", "4",
                     "--metric_binary_folder", "no_metric_binary_for_testing"),
            0);
}

TEST(CodecCompareGenTest, MissingFlags) {
  EXPECT_EQ(TestMain(data_path), 1);
  EXPECT_EQ(TestMain("--lossy"), 1);
  EXPECT_EQ(TestMain(data_path, "--lossless"), 1);
  EXPECT_EQ(TestMain(data_path, "--lossy"), 1);
  EXPECT_EQ(TestMain(data_path, "--lossy", "--metric_binary_folder",
                     "no_metric_binary_for_testing"),
            1);
}

template <class... Args>
void TestProgressFileLength(size_t expected_lines, Args... args) {
  const std::string progress_file_path =
      std::filesystem::path(::testing::TempDir()) /
      ("progress" + std::to_string(expected_lines) + ".csv");
  (void)std::filesystem::remove(progress_file_path);

  std::vector<std::string> paths = {"alpha1x17.png", "anim80x80.gif",
                                    "anim80x80.webp", "gradient32x32.png"};
  for (std::string& path : paths) path = data_path + path;
  EXPECT_EQ(TestMain(paths[0].c_str(), paths[1].c_str(), paths[2].c_str(),
                     paths[3].c_str(), "--codec", "webp", "420", "4",
                     "--metric_binary_folder", "no_metric_binary_for_testing",
                     "--progress_file", progress_file_path.c_str(), args...),
            0);

  std::ifstream file(progress_file_path);
  std::vector<std::string> lines;
  for (std::string line; std::getline(file, line);) lines.push_back(line);
  EXPECT_EQ(lines.size(), expected_lines);
}

TEST(CodecCompareGenTest, Qualities) {
  constexpr size_t num_img = 4;
  TestProgressFileLength(/*expected_lines=*/num_img * 1, "--qualities", "10");
  TestProgressFileLength(/*expected_lines=*/num_img * 2, "--qualities", "10",
                         "--qualities", "52");
  TestProgressFileLength(/*expected_lines=*/num_img * 10, "--qualities",
                         "10:19");
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
