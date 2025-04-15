// Copyright 2025 Google LLC
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

#include <cstring>
#include <filesystem>
#include <iostream>

#include "src/base.h"
#include "src/distortion.h"
#include "src/frame.h"

namespace codec_compare_gen {

void PrintUsage(const char* binary_path) {
  std::cout << "Usage: "
            << std::filesystem::path(binary_path).filename().string()
            << " <path> <path>" << std::endl;
}

StatusOr<bool> AreEquivalent(const char* file_path_a, const char* file_path_b) {
  ASSIGN_OR_RETURN(Image image_a, ReadStillImageOrAnimation(
                                      file_path_a, kARGB32, /*quiet=*/false));
  ASSIGN_OR_RETURN(Image image_b, ReadStillImageOrAnimation(
                                      file_path_b, kARGB32, /*quiet=*/false));
  return PixelEquality(image_a, image_b, /*quiet=*/false);
}

int Main(int argc, const char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "-h") || !std::strcmp(argv[i], "--help")) {
      std::cout << "Checks if the files at the given paths have the same pixel "
                   "values."
                << std::endl;
      PrintUsage(argv[0]);
      return 0;
    }
  }
  if (argc != 3) {
    std::cerr << "Wrong number of arguments." << std::endl;
    PrintUsage(argv[0]);
    return 1;
  }

  const StatusOr<bool> result = AreEquivalent(argv[1], argv[2]);
  if (result.status != Status::kOk) {
    std::cerr << "Failed to open " << argv[1] << " or " << argv[2] << std::endl;
    return 1;
  }
  if (!result.value) {
    std::cout << argv[1] << " differs from " << argv[2] << std::endl;
    return 1;
  }
  return 0;
}

}  // namespace codec_compare_gen

int main(int argc, const char* argv[]) {
  return codec_compare_gen::Main(argc, argv);
}
