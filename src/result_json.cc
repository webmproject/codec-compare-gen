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

#include <algorithm>
#include <cassert>
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

// Returns the path of the deepest folder containing all assets.
std::filesystem::path GetCommonParent(const std::vector<TaskOutput>& tasks,
                                      bool get_encoded_path) {
  std::filesystem::path prefix;
  for (size_t i = 0; i < tasks.size(); ++i) {
    const std::filesystem::path parent =
        std::filesystem::path(get_encoded_path
                                  ? tasks[i].task_input.encoded_path
                                  : tasks[i].task_input.image_path)
            .parent_path();
    if (i == 0) {
      prefix = parent;
    } else {
      const auto mismatch = std::mismatch(parent.begin(), parent.end(),
                                          prefix.begin(), prefix.end())
                                .second;
      if (mismatch != prefix.end()) {
        // parent does not begin as prefix.
        std::filesystem::path common_prefix;
        for (auto it = prefix.begin(); it != mismatch; ++it) {
          common_prefix.append(it->string());
        }
        prefix = common_prefix;
      }
    }
  }
  return prefix;
}

std::filesystem::path RemovePrefix(const std::filesystem::path& prefix,
                                   const std::filesystem::path& path) {
  std::filesystem::path stripped_path;
  auto prefix_it = prefix.begin();
  for (auto path_element : path) {
    if (prefix_it != prefix.end()) {
      assert(path_element.string() == prefix_it->string());
      ++prefix_it;
    } else {
      stripped_path.append(path_element.string());
    }
  }
  return stripped_path;
}

std::string AppendDirectorySeparator(const std::filesystem::path& path) {
  const std::filesystem::path fake_file("fake_file");
  const std::string path_with_fake_file = (path / fake_file).string();
  return path_with_fake_file.substr(
      0, path_with_fake_file.size() - fake_file.string().size());
}

}  // namespace

Status TasksToJson(const std::string& batch_pretty_name, CodecSettings settings,
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

  // Keep only the file name as original_name, eventually with any leading
  // differentiating parent folders. Find out what to strip.
  const std::filesystem::path image_common_parent =
      GetCommonParent(tasks, /*get_encoded_path=*/false);
  const std::filesystem::path encoded_common_parent =
      GetCommonParent(tasks, /*get_encoded_path=*/true);
  // Only keep the relative parent folder as original_path root. The full
  // absolute path is less likely to be useful.
  const std::string image_parent = AppendDirectorySeparator(RemovePrefix(
      /*prefix=*/image_common_parent.parent_path(), image_common_parent));
  const std::string encoded_parent = AppendDirectorySeparator(RemovePrefix(
      /*prefix=*/encoded_common_parent.parent_path(), encoded_common_parent));

  const std::string deps_extra_step =
      settings.codec == Codec::kAvifAvm
          ? " && mv third_party/libavif third_party/libavif_aom"
            " && mv third_party/libavif_avm third_party/libavif"
          : "";
  const std::string build_cmd =
      "git clone -b v0.5.5 --depth 1"
      " https://github.com/webmproject/codec-compare-gen.git"
      " && cd codec-compare-gen && ./deps.sh" +
      deps_extra_step +
      " && cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++"
      " && cmake --build build --parallel && cd ..";
  const std::string effort_str =
      (settings.codec == Codec::kWebp || settings.codec == Codec::kWebp2 ||
       settings.codec == Codec::kJpegXl || settings.codec == Codec::kAvif ||
       settings.codec == Codec::kAvifExp || settings.codec == Codec::kAvifAvm ||
       settings.codec == Codec::kCombination ||
       settings.codec == Codec::kJpegsimple)
          ? " " + std::to_string(settings.effort)
          : "";  // kJpegturbo, kJpegli, and kJpegmoz have no effort setting.
  std::string encoding_cmd =
      "codec-compare-gen/build/ccgen --codec " + CodecName(settings.codec) +
      " " + SubsamplingToString(settings.chroma_subsampling) + effort_str;
  if (settings.quality == kQualityLossless) {
    encoding_cmd += " --lossless";
  } else {
    encoding_cmd += " --lossy --quality ${quality}";
    encoding_cmd += " --metric_binary_folder codec-compare-gen/third_party/";
  }
  encoding_cmd += " -- ${original_name}";

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
       << Escape(batch_pretty_name) << R"json(,
    )json"
       << Escape(CodecName(settings.codec)) << R"json(,
    )json"
       << Escape(CodecVersion(settings.codec) + "_" +
                 SubsamplingToString(settings.chroma_subsampling))
       << R"json(,
    )json"
       << Escape(DateTime()) << R"json(,
    )json"
       << Escape(image_parent + "${original_name}") << R"json(,
    )json"
       << Escape(build_cmd) << R"json(,
    )json"
       << Escape(encoding_cmd);
  if (has_encoded_path) {
    file << R"json(,
    )json"
         << Escape(encoded_parent + "${encoded_name}");
  }
  if (has_decoded_path) {
    file << R"json(,
    )json"
         << Escape(encoded_parent + "${encoded_name}.png");
  }
  file << R"json(
  ],)json";

  file << R"json(
  "field_descriptions": [
    {"original_name": "Original image file name"},
    {"width": "Pixel columns in the image that was encoded"},
    {"height": "Pixel rows in the image that was encoded"},
    {"depth": "Bit depth of the image that was encoded"},
    {"frame_count": "Number of frames in the image that was encoded"},)json";
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
    file << Escape(
                RemovePrefix(/*prefix=*/image_common_parent,
                             std::filesystem::path(task.task_input.image_path))
                    .string())
         << ",";
    file << task.image_width << ",";
    file << task.image_height << ",";
    file << task.bit_depth << ",";
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
      file << Escape(RemovePrefix(
                         /*prefix=*/encoded_common_parent,
                         std::filesystem::path(task.task_input.encoded_path))
                         .string())
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
