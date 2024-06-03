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

// Command line wrapper around framework.h.

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "src/base.h"
#include "src/codec.h"
#include "src/framework.h"

namespace {

void GetAllFilesIn(const std::string& file_or_directory_path,
                   std::vector<std::string>& file_paths) {
  if (std::filesystem::is_directory(file_or_directory_path)) {
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(file_or_directory_path)) {
      GetAllFilesIn(entry.path(), file_paths);
    }
  } else {
    file_paths.push_back(file_or_directory_path);
  }
}

struct CodecEffort {
  codec_compare_gen::Codec codec;
  int effort;
};

}  // namespace

int main(int argc, char* argv[]) {
  std::vector<std::string> image_paths;
  std::vector<CodecEffort> codec_settings;
  codec_compare_gen::ComparisonSettings settings;
  bool lossy = false;
  bool lossless = false;
  std::unordered_set<int> allowed_qualities;
  std::string completed_tasks_file_path;
  std::string results_folder_path;

  settings.random_order = true;
  settings.quiet = false;

  int arg_index = 1;
  for (; arg_index < argc; ++arg_index) {
    const std::string arg = argv[arg_index];
    if (arg == "-h" || arg == "--help") {
      std::cout << "Usage: " << argv[0] << std::endl
                << " [--codec webp {effort}]" << std::endl
                << " [--codec webp2 {effort}]" << std::endl
                << " [--codec jpegxl {effort}]" << std::endl
                << " --lossy|--lossless" << std::endl
                << " [--qualities {unique|min:max}]"
                << " [--repeat {number of times to encode each image}]"
                << std::endl
                << " [--recompute_distortion]" << std::endl
                << " [--threads {extra threads on top of main thread}]"
                << std::endl
                << " [--deterministic]" << std::endl
                << " [--quiet]" << std::endl
                << " [--metric_binary_folder {path to third_party created by "
                   "deps.sh}]"
                << std::endl
                << " [--encoded_folder {path}]" << std::endl
                << " --progress_file {path}" << std::endl
                << " --results_folder {path}" << std::endl
                << " --" << std::endl
                << " {image file path}..." << std::endl;
      return 0;
    } else if (arg == "--codec" && arg_index + 2 < argc) {
      const std::string codec = argv[++arg_index];
      const int effort = std::stoi(argv[++arg_index]);
      if (codec == "webp") {
        codec_settings.push_back({codec_compare_gen::Codec::kWebp, effort});
      } else if (codec == "wp2" || codec == "webp2") {
        codec_settings.push_back({codec_compare_gen::Codec::kWebp2, effort});
      } else if (codec == "jxl" || codec == "jpegxl") {
        codec_settings.push_back({codec_compare_gen::Codec::kJpegXl, effort});
      } else if (codec == "avif") {
        codec_settings.push_back({codec_compare_gen::Codec::kAvif, effort});
      } else if (codec == "combination") {
        codec_settings.push_back(
            {codec_compare_gen::Codec::kCombination, effort});
      } else {
        std::cerr << "Error: Unknown codec \"" << codec << "\"" << std::endl;
        return 1;
      }
    } else if (arg == "--repeat" && arg_index + 1 < argc) {
      settings.num_repetitions = std::stoul(argv[++arg_index]);
    } else if (arg == "--recompute_distortion") {
      settings.discard_distortion_values = true;
    } else if (arg == "--lossy") {
      lossy = true;
    } else if (arg == "--lossless") {
      lossless = true;
    } else if (arg == "--qualities" && arg_index + 1 < argc) {
      const std::string str = argv[++arg_index];
      const auto range_delim = str.find(':');
      if (range_delim != std::string::npos) {
        allowed_qualities.insert(std::stoi(str.substr(0, range_delim)));
        allowed_qualities.insert(std::stoi(str.substr(range_delim + 1)));
      } else {
        allowed_qualities.insert(std::stoi(str));
      }
      lossy = true;
    } else if (arg == "--threads" && arg_index + 1 < argc) {
      settings.num_extra_threads = std::stoul(argv[++arg_index]);
    } else if (arg == "--deterministic") {
      settings.random_order = false;
    } else if (arg == "--quiet") {
      settings.quiet = true;
    } else if (arg == "--metric_binary_folder" && arg_index + 1 < argc) {
      settings.metric_binary_folder_path = argv[++arg_index];
    } else if (arg == "--encoded_folder" && arg_index + 1 < argc) {
      settings.encoded_folder_path = argv[++arg_index];
    } else if (arg == "--progress_file" && arg_index + 1 < argc) {
      completed_tasks_file_path = argv[++arg_index];
    } else if (arg == "--results_folder" && arg_index + 1 < argc) {
      results_folder_path = argv[++arg_index];
    } else if (arg == "--") {
      ++arg_index;
      break;
    } else {
      if (!arg.empty() && arg[0] == '-') {
        std::cerr << "Error: Unknown argument \"" << arg << "\""
                  << " (prepend -- to consider it as a file path)" << std::endl;
        return 1;
      }
      GetAllFilesIn(arg, image_paths);
    }
  }

  if (!(lossy ^ lossless)) {
    std::cerr << "There must be --lossy/--qualities or --lossless but not both"
              << std::endl;
    return 1;
  }
  if (lossy && settings.metric_binary_folder_path.empty()) {
    std::cerr << "Missing --metric_binary_folder for lossy evaluations"
              << std::endl;
    return 1;
  }

  // All arguments after "--" are file paths.
  for (; arg_index < argc; ++arg_index) {
    GetAllFilesIn(argv[arg_index], image_paths);
  }

  if (lossy) {
    std::vector<std::vector<int>> qualities(
        static_cast<int>(codec_compare_gen::Codec::kCombination) + 1);
    for (size_t i = 0; i < qualities.size(); ++i) {
      qualities[i] = codec_compare_gen::CodecLossyQualities(
          static_cast<codec_compare_gen::Codec>(i));
    }
    for (const CodecEffort& setting : codec_settings) {
      for (const int quality : qualities[static_cast<int>(setting.codec)]) {
        if (allowed_qualities.empty() ||
            allowed_qualities.find(quality) != allowed_qualities.end()) {
          settings.codec_settings.push_back(
              {setting.codec, setting.effort, quality});
        }
      }
    }
  } else {
    for (const CodecEffort& setting : codec_settings) {
      settings.codec_settings.push_back(
          {setting.codec, setting.effort, codec_compare_gen::kQualityLossless});
    }
  }

  if (codec_compare_gen::Compare(
          image_paths, settings, completed_tasks_file_path,
          results_folder_path) != codec_compare_gen::Status::kOk) {
    return 1;
  }
  return 0;
}
