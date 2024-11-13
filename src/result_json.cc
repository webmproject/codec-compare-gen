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

#include "src/result_json.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "src/base.h"
#include "src/codec.h"
#include "src/framework.h"
#include "src/serialization.h"
#include "src/task.h"

namespace codec_compare_gen {

namespace {

std::string DateTime() {
  const std::time_t time = std::time(nullptr);
  const std::tm localtime = *std::localtime(&time);
  return (std::ostringstream()
          << std::put_time(&localtime, "%Y-%m-%dT%H:%M:%S"))
      .str();
}

const std::string& GetPath(const TaskOutput& task, bool get_encoded_path) {
  return get_encoded_path ? task.task_input.encoded_path
                          : task.task_input.image_path;
}

std::string GetImagePathCommonPrefix(const std::vector<TaskOutput>& tasks,
                                     bool get_encoded_path) {
  if (tasks.empty()) return "";

  // Return longest common prefix, even if it contains a part of the file name.
  std::string prefix = GetPath(tasks.front(), get_encoded_path);
  for (size_t i = 1; i < tasks.size(); ++i) {
    const std::string& path = GetPath(tasks[i], get_encoded_path);
    if (path.size() < prefix.size()) prefix.resize(path.size());
    for (size_t c = 0; c < prefix.size(); ++c) {
      if (prefix[c] != path[c]) prefix.resize(c);
    }
  }

  if (prefix == GetPath(tasks.front(), get_encoded_path)) {
    // Split at last "/" if the prefix includes everything.
    const std::string& path = GetPath(tasks.front(), get_encoded_path);
    prefix = path.substr(
        /*pos=*/0, /*n=*/path.size() -
                       std::filesystem::path(path).filename().string().size());
  }

  return prefix;
}

}  // namespace

Status TasksToJson(const std::string& batch_name, CodecSettings settings,
                   const std::vector<TaskOutput>& tasks, bool quiet,
                   const std::string& results_file_path) {
  bool lossless = true;
  bool has_encoded_path = true;
  for (size_t i = 0; i < tasks.size(); ++i) {
    const CodecSettings& codec_settings = tasks[i].task_input.codec_settings;
    CHECK_OR_RETURN(
        codec_settings.codec == settings.codec &&
            codec_settings.chroma_subsampling == settings.chroma_subsampling &&
            codec_settings.effort == settings.effort,
        quiet)
        << "Codec settings do not match";
    lossless &= codec_settings.quality == kQualityLossless;
    has_encoded_path &= !tasks[i].task_input.encoded_path.empty();
  }

  // See EncodeDecode().
  const bool has_decoded_path =
      has_encoded_path && !CodecIsSupportedByBrowsers(settings.codec);

  std::ofstream file(results_file_path, std::ios::trunc);
  CHECK_OR_RETURN(file.is_open(), quiet) << "Failed to open results file at "
                                         << results_file_path << " for writing";

  const std::string image_prefix =
      GetImagePathCommonPrefix(tasks, /*get_encoded_path=*/false);
  const std::string build_cmd =
      "git clone -b v0.3.5 --depth 1"
      " https://github.com/webmproject/codec-compare-gen.git &&"
      " cd codec-compare-gen && ./deps.sh &&"
      " cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++ &&"
      " cmake --build build && cd ..";
  std::string encoding_cmd = "codec-compare-gen/build/ccgen --codec " +
                             CodecName(settings.codec) + " " +
                             SubsamplingToString(settings.chroma_subsampling) +
                             " " + std::to_string(settings.effort);
  if (settings.quality == kQualityLossless) {
    encoding_cmd += " --lossless";
  } else {
    encoding_cmd += " --lossy --quality " + std::to_string(settings.quality);
    encoding_cmd += " --metric_binary_folder codec-compare-gen/third_party/";
  }
  encoding_cmd += " -- ${original_path}";
  const std::string encoded_prefix =
      GetImagePathCommonPrefix(tasks, /*get_encoded_path=*/true);

  file << R"json({
  "constant_descriptions": [
    {"name": "Name of this batch"},
    {"codec": "Name of the codec used to generate this data"},
    {"version": "Version of the codec used to generate this data"},
    {"time": "Timestamp of when this data was generated"},
    {"original_path": "Path to the original image"},
    {"build_command": "The command used to generate the codec binaries"},
    {"encoding_cmd": "The command used to encode the original image"})json";
  if (has_encoded_path) {
    file << R"json(,
    {"encoded_path": "Path to the encoded image"})json";
  }
  if (has_decoded_path) {
    file << R"json(,
    {"decoded_path": "Path to the decoded image"})json";
  }
  file << R"json(
  ],
  "constant_values": [
    )json"
       << Escape(batch_name) << R"json(,
    )json"
       << Escape(CodecName(settings.codec)) << R"json(,
    )json"
       << Escape(CodecVersion(settings.codec)) << R"json(,
    )json"
       << Escape(DateTime()) << R"json(,
    )json"
       << Escape(image_prefix + "${original_name}") << R"json(,
    )json"
       << Escape(build_cmd) << R"json(,
    )json"
       << Escape(encoding_cmd);
  if (has_encoded_path) {
    file << R"json(,
    )json"
         << Escape(encoded_prefix + "${encoded_name}");
  }
  if (has_decoded_path) {
    file << R"json(,
    )json"
         << Escape(encoded_prefix + "${encoded_name}.png");
  }
  file << R"json(
  ],)json";

  file << R"json(
  "field_descriptions": [
    {"original_name": "Original image file name"},
    {"width": "Pixel columns in the original image"},
    {"height": "Pixel rows in the original image"},
    {"frame_count": "Number of frames in the original image"},)json";
  if (!lossless) {
    file << R"json(
    {"chroma_subsampling": "Compression chroma subsampling parameter"},)json";
  }
  file << R"json(
    {"effort": "Compression effort parameter"},)json";
  if (!lossless) {
    file << R"json(
    {"quality": "Compression quality parameter"},)json";
  }
  if (has_encoded_path) {
    file << R"json(
    {"encoded_name": "Name of the encoded image"},)json";
  }
  file << R"json(
    {"encoded_size": "Size of the encoded image file in bytes"},
    {"encoding_time": "Encoding duration in seconds. Warning: Timings are environment-dependent and inaccurate."},
    {"decoding_time": "Decoding duration in seconds. Warning: Timings are environment-dependent and inaccurate."},
    {"dec_time_no_col_conv": "Decoding duration in seconds without color conversion. Warning: Only different from regular decoding for codecs without built-in conversion."})json";
  if (!lossless) {
    static_assert(kNumDistortionMetrics == 7);
    // In DistortionMetric order.
    file << R"json(,
    {"psnr": "Distortion metric Peak Signal-to-Noise Ratio (libwebp2 implementation). See https://en.wikipedia.org/wiki/Peak_signal-to-noise_ratio. Warning: There is no scientific consensus on which objective distortion metric to use."},
    {"ssim": "Distortion metric Structural Similarity Index Measure (libwebp2 implementation). See https://en.wikipedia.org/wiki/Structural_similarity. Warning: There is no scientific consensus on which objective distortion metric to use."},
    {"dssim": "Distortion metric Structural Dissimilarity (kornelski implementation). See https://en.wikipedia.org/wiki/Structural_similarity_index_measure#Structural_Dissimilarity. Warning: There is no scientific consensus on which objective distortion metric to use."},
    {"butteraugli": "Distortion metric Butteraugli (libjxl implementation). See https://en.wikipedia.org/wiki/Guetzli#Butteraugli. Warning: There is no scientific consensus on which objective distortion metric to use."},
    {"ssimulacra": "Distortion metric SSIMULACRA (libjxl implementation). See https://en.wikipedia.org/wiki/Structural_similarity#SSIMULACRA. Warning: There is no scientific consensus on which objective distortion metric to use."},
    {"ssimulacra2": "Distortion metric SSIMULACRA2 (libjxl implementation). See https://en.wikipedia.org/wiki/Structural_similarity#SSIMULACRA. Warning: There is no scientific consensus on which objective distortion metric to use."},
    {"p3norm": "Distortion metric P3-norm (libjxl implementation). See https://en.wikipedia.org/wiki/Norm_(mathematics)#p-norm. Warning: There is no scientific consensus on which objective distortion metric to use."})json";
  }
  file << R"json(
  ],
  "field_values": [
)json";

  for (size_t i = 0; i < tasks.size(); ++i) {
    const TaskOutput& task = tasks[i];
    file << "    [";
    file << Escape(task.task_input.image_path.substr(image_prefix.size()))
         << ",";
    file << task.image_width << ",";
    file << task.image_height << ",";
    file << task.num_frames << ",";
    if (!lossless) {
      file << SubsamplingToString(
                  task.task_input.codec_settings.chroma_subsampling)
           << ",";
    }
    file << task.task_input.codec_settings.effort << ",";
    if (!lossless) {
      file << task.task_input.codec_settings.quality << ",";
    }
    if (has_encoded_path) {
      file << Escape(task.task_input.encoded_path.substr(encoded_prefix.size()))
           << ",";
    }
    file << task.encoded_size << ",";
    file << task.encoding_duration << ",";
    file << task.decoding_duration << ",";
    file << (task.decoding_duration - task.decoding_color_conversion_duration);
    if (!lossless) {
      for (const float distortion : task.distortions) {
        file << "," << distortion;
      }
    }
    file << "]";
    if (i + 1 < tasks.size()) file << ",";
    file << std::endl;
  }

  file << "  ]" << std::endl << "}" << std::endl;
  file.close();
  return Status::kOk;
}

}  // namespace codec_compare_gen
