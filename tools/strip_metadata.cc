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
#include "src/frame.h"

namespace codec_compare_gen {

void PrintHelp(const char* binary_path) {
  std::cout << "Strips any metadata from PNG still images and WebP animations "
               "in place."
            << std::endl
            << "Usage: "
            << std::filesystem::path(binary_path).filename().string()
            << " <path>..." << std::endl;
}

Status StripMetadata(const char* file_path) {
  ASSIGN_OR_RETURN(Image image, ReadStillImageOrAnimation(file_path, kARGB32,
                                                          /*quiet=*/false));
  for (auto& frame : image) {
    frame.pixels.metadata_.Clear();
  }
  OK_OR_RETURN(WriteStillImageOrAnimation(image, file_path, /*quiet=*/false));
  return Status::kOk;
}

int Main(int argc, const char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "-h") || !std::strcmp(argv[i], "--help")) {
      PrintHelp(argv[0]);
      return 0;
    }
  }

  for (int i = 1; i < argc; ++i) {
    if (StripMetadata(argv[i]) != Status::kOk) {
      std::cerr << "Failed to open or save " << argv[i] << std::endl;
      return 1;
    }
  }
  return 0;
}

}  // namespace codec_compare_gen

int main(int argc, const char* argv[]) {
  return codec_compare_gen::Main(argc, argv);
}
