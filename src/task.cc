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

#include "src/task.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/base.h"
#include "src/codec.h"
#include "src/framework.h"
#include "src/serialization.h"

namespace codec_compare_gen {

namespace {

std::string GetEncodedFilePath(const std::string& folder_path,
                               const std::string& image_path,
                               const CodecSettings& codec_settings) {
  if (folder_path.empty()) return "";

  std::filesystem::path path(folder_path);
  path.append(std::filesystem::path(image_path).filename().string());

  std::stringstream ext;
  ext << ".";
  if (codec_settings.quality != kQualityLossless ||
      codec_settings.chroma_subsampling != Subsampling::k444) {
    // 444/420 could be prepended by "yuv" but it makes the file name longer and
    // it could be misleading for RGB 444.
    ext << SubsamplingToString(codec_settings.chroma_subsampling);
  }
  ext << "e" << codec_settings.effort;
  if (codec_settings.quality == kQualityLossless) {
    ext << "lossless";
  } else {
    ext << "q" << std::setfill('0') << std::setw(3) << codec_settings.quality;
  }
  ext << "." << CodecExtension(codec_settings.codec);
  path.replace_extension(ext.str());
  return path;
}

bool operator==(const CodecSettings& a, const CodecSettings& b) {
  return a.codec == b.codec && a.chroma_subsampling == b.chroma_subsampling &&
         a.effort == b.effort && a.quality == b.quality;
}

}  // namespace

bool operator==(const TaskInput& a, const TaskInput& b) {
  return a.codec_settings == b.codec_settings && a.image_path == b.image_path &&
         a.encoded_path == b.encoded_path;
}

//------------------------------------------------------------------------------
// Task serialization

std::string TaskOutput::Serialize() const {
  std::stringstream ss;
  ss << Escape(CodecName(task_input.codec_settings.codec)) << ", "
     << SubsamplingToString(task_input.codec_settings.chroma_subsampling)
     << ", " << task_input.codec_settings.effort << ", "
     << task_input.codec_settings.quality << ", "
     << Escape(task_input.image_path) << ", " << image_width << ", "
     << image_height << ", " << num_frames << ", "
     << Escape(task_input.encoded_path) << ", " << encoded_size << ", "
     << encoding_duration << ", " << decoding_duration << ", "
     << decoding_color_conversion_duration;
  if (task_input.codec_settings.quality != kQualityLossless) {
    for (size_t metric = 0; metric < kNumDistortionMetrics; ++metric) {
      ss << ", " << distortions[metric];
    }
  }
  return ss.str();
}

namespace {

constexpr size_t kNumNonDistortionTokens = 13;

StatusOr<TaskOutput> UnserializeNoDistortion(
    const std::string& serialized_task, const std::vector<std::string> tokens,
    bool quiet) {
  CHECK_OR_RETURN(tokens.size() >= kNumNonDistortionTokens, quiet)
      << "Expected " << kNumNonDistortionTokens << "+ tokens in \""
      << serialized_task << "\" but found " << tokens.size();
  size_t t = 0;

  TaskOutput task;
  ASSIGN_OR_RETURN(const std::string codec_name, Unescape(tokens[t++], quiet));
  ASSIGN_OR_RETURN(task.task_input.codec_settings.codec,
                   CodecFromName(codec_name, quiet));

  ASSIGN_OR_RETURN(task.task_input.codec_settings.chroma_subsampling,
                   SubsamplingFromString(tokens[t++], quiet));

  task.task_input.codec_settings.effort = std::stoul(tokens[t++]);
  CHECK_OR_RETURN(task.task_input.codec_settings.effort <= 10, quiet)
      << "Unknown effort in \"" << serialized_task << "\"";

  task.task_input.codec_settings.quality = std::stoi(tokens[t++]);
  CHECK_OR_RETURN(task.task_input.codec_settings.quality == kQualityLossless ||
                      (task.task_input.codec_settings.quality >= 0 &&
                       task.task_input.codec_settings.quality <= 100),
                  quiet)
      << "Unknown quality in \"" << serialized_task << "\"";

  ASSIGN_OR_RETURN(task.task_input.image_path, Unescape(tokens[t++], quiet));
  task.image_width = std::stoul(tokens[t++]);
  task.image_height = std::stoul(tokens[t++]);
  task.num_frames = std::stoul(tokens[t++]);

  ASSIGN_OR_RETURN(task.task_input.encoded_path, Unescape(tokens[t++], quiet));
  task.encoded_size = std::stoul(tokens[t++]);
  task.encoding_duration = std::stod(tokens[t++]);
  task.decoding_duration = std::stod(tokens[t++]);
  task.decoding_color_conversion_duration = std::stod(tokens[t++]);

  CHECK_OR_RETURN(t == kNumNonDistortionTokens, quiet);

  CHECK_OR_RETURN(
      task.image_width > 0 && task.image_height > 0 && task.num_frames > 0,
      quiet)
      << "Bad image dimensions in \"" << serialized_task << "\"";
  CHECK_OR_RETURN(task.encoded_size > 0, quiet)
      << "Bad encoded size in \"" << serialized_task << "\"";
  CHECK_OR_RETURN(task.encoding_duration > 0, quiet)
      << "Bad encoded duration in \"" << serialized_task << "\"";
  CHECK_OR_RETURN(task.decoding_duration > 0, quiet)
      << "Bad decoded duration in \"" << serialized_task << "\"";
  CHECK_OR_RETURN(task.decoding_duration >= 0, quiet)
      << "Bad color conversion duration in \"" << serialized_task << "\"";
  return task;
}

}  // namespace

StatusOr<TaskOutput> TaskOutput::UnserializeNoDistortion(
    const std::string& serialized_task, bool quiet) {
  return ::codec_compare_gen::UnserializeNoDistortion(
      serialized_task, Split(serialized_task, ','), quiet);
}

StatusOr<TaskOutput> TaskOutput::Unserialize(const std::string& serialized_task,
                                             bool quiet) {
  const std::vector<std::string> tokens = Split(serialized_task, ',');
  ASSIGN_OR_RETURN(TaskOutput task,
                   ::codec_compare_gen::UnserializeNoDistortion(serialized_task,
                                                                tokens, quiet));
  if (tokens.size() == kNumNonDistortionTokens) {
    // Likely lossless.
    std::fill(task.distortions, task.distortions + kNumDistortionMetrics,
              kNoDistortion);
  } else {
    CHECK_OR_RETURN(
        tokens.size() == kNumNonDistortionTokens + kNumDistortionMetrics, quiet)
        << "Expected " << kNumNonDistortionTokens + kNumDistortionMetrics
        << " tokens instead of " << tokens.size() << " in \"" << serialized_task
        << "\", try the flag --recompute_distortion";

    for (size_t metric = 0; metric < kNumDistortionMetrics; ++metric) {
      task.distortions[metric] =
          std::stof(tokens[kNumNonDistortionTokens + metric]);
      if (metric != static_cast<size_t>(DistortionMetric::kLibjxlButteraugli) &&
          metric != static_cast<size_t>(DistortionMetric::kLibjxlSsimulacra2)) {
        CHECK_OR_RETURN(task.distortions[metric] <= 99, quiet)
            << "Bad " << kDistortionMetricToStr[metric] << " metric value "
            << task.distortions[metric] << " in \"" << serialized_task << "\"";
      }
    }
  }
  return task;
}

//------------------------------------------------------------------------------
// Task generation and aggregation

StatusOr<std::vector<TaskInput>> PlanTasks(
    const std::vector<std::string>& image_paths,
    const ComparisonSettings& settings) {
  CHECK_OR_RETURN(!image_paths.empty(), settings.quiet)
      << "No specified input image file path";
  CHECK_OR_RETURN(!settings.codec_settings.empty(), settings.quiet)
      << "No specified codec";

  std::vector<TaskInput> tasks;
  tasks.reserve(settings.codec_settings.size() * image_paths.size() *
                (1 + settings.num_repetitions));
  for (const CodecSettings& codec_settings : settings.codec_settings) {
    for (const std::string& image_path : image_paths) {
      const std::string encoded_file_path = GetEncodedFilePath(
          settings.encoded_folder_path, image_path, codec_settings);
      for (uint32_t i = 0; i < 1 + settings.num_repetitions; ++i) {
        tasks.push_back(
            TaskInput{codec_settings, image_path, encoded_file_path});
      }
    }
  }
  return tasks;
}

namespace {

// Returns true if a and b can be considered the same amount of loss.
bool SameDistortion(float a, float b) {
  if (a >= kNoDistortion) return b >= kNoDistortion;
  if (b >= kNoDistortion) return false;
  return std::abs(a - b) < 0.001f;
}

// Returns true if a and b are repetitions of the same task.
bool TaskOutputsAreRepetitions(const TaskOutput& a, const TaskOutput& b) {
  if (!(a.task_input == b.task_input)) return false;
  if (a.image_width != b.image_width) return false;
  if (a.image_height != b.image_height) return false;
  if (a.num_frames != b.num_frames) return false;
  if (a.encoded_size != b.encoded_size) return false;
  for (size_t metric = 0; metric < kNumDistortionMetrics; ++metric) {
    if (!SameDistortion(a.distortions[metric], b.distortions[metric])) {
      return false;
    }
  }
  return true;
}

StatusOr<std::vector<TaskOutput>> AggregateResultsByImageAndQuality(
    const std::vector<TaskOutput>& results, bool quiet) {
  struct AggregatedTaskOutput {
    TaskOutput task_output;
    uint32_t count;
  };
  std::unordered_map<std::string, std::unordered_map<int, AggregatedTaskOutput>>
      image_and_quality_to_results;
  for (const TaskOutput& result : results) {
    std::unordered_map<int, AggregatedTaskOutput>& quality_to_results =
        image_and_quality_to_results[result.task_input.image_path];
    auto [it, was_inserted] =
        quality_to_results.emplace(result.task_input.codec_settings.quality,
                                   AggregatedTaskOutput{result, /*count=*/1});
    TaskOutput& task_output = it->second.task_output;
    if (!was_inserted) {
      CHECK_OR_RETURN(TaskOutputsAreRepetitions(task_output, result), quiet)
          << task_output.Serialize() << " != " << result.Serialize();
      task_output.encoding_duration += result.encoding_duration;
      task_output.decoding_duration += result.decoding_duration;
      task_output.decoding_color_conversion_duration +=
          result.decoding_color_conversion_duration;
      ++it->second.count;
    }
  }

  std::vector<TaskOutput> aggregated_results;
  aggregated_results.reserve(image_and_quality_to_results.size());
  for (const auto& [image, qualities] : image_and_quality_to_results) {
    for (const auto& [quality, aggregated_rows] : qualities) {
      aggregated_results.push_back(aggregated_rows.task_output);
      aggregated_results.back().encoding_duration /= aggregated_rows.count;
      aggregated_results.back().decoding_duration /= aggregated_rows.count;
      aggregated_results.back().decoding_color_conversion_duration /=
          aggregated_rows.count;
    }
  }
  return aggregated_results;
}

}  // namespace

StatusOr<std::vector<std::vector<TaskOutput>>>
SplitByCodecSettingsAndAggregateByImageAndQuality(
    const std::vector<TaskOutput>& results, bool quiet) {
  std::vector<std::vector<TaskOutput>> aggregated_results;

  auto cmp = [](const CodecSettings& a, const CodecSettings& b) {
    // Multiple qualities can coexist in the same aggregate (meaning in the same
    // output JSON single file). Only split by codec, chroma subsampling and
    // effort.
    return a.codec < b.codec ||
           (a.codec == b.codec &&
            a.chroma_subsampling < b.chroma_subsampling) ||
           (a.codec == b.codec &&
            a.chroma_subsampling == b.chroma_subsampling &&
            a.effort < b.effort);
  };
  std::map<CodecSettings, std::vector<TaskOutput>, decltype(cmp)> map(cmp);
  for (const TaskOutput& result : results) {
    map[result.task_input.codec_settings].push_back(result);
  }

  aggregated_results.reserve(map.size());
  for (const auto& [codec_settings, results] : map) {
    aggregated_results.push_back({});
    std::vector<TaskOutput>& aggregate = aggregated_results.back();
    ASSIGN_OR_RETURN(aggregate,
                     AggregateResultsByImageAndQuality(results, quiet));

    // codec, chroma subsampling and effort are the same in these results so
    // only sort by original image name and quality.
    std::sort(aggregate.begin(), aggregate.end(),
              [](const TaskOutput& a, const TaskOutput& b) {
                return a.task_input.image_path < b.task_input.image_path ||
                       (a.task_input.image_path == b.task_input.image_path &&
                        a.task_input.codec_settings.quality <
                            b.task_input.codec_settings.quality);
              });
  }
  return aggregated_results;
}

}  // namespace codec_compare_gen
