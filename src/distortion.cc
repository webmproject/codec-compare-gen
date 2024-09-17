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

#include "src/distortion.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include "src/base.h"
#include "src/codec.h"
#include "src/frame.h"
#include "src/framework.h"
#include "src/serialization.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/imageio/image_enc.h"
#include "third_party/libwebp2/src/wp2/base.h"
#endif  // HAS_WEBP2

namespace codec_compare_gen {

#if defined(HAS_WEBP2)

namespace {

class FileDeleter {
 public:
  explicit FileDeleter(const std::string& file_path) : file_path_(file_path) {}
  ~FileDeleter() {
    if (!file_path_.empty()) std::filesystem::remove(file_path_);
  }
  const std::string file_path_;
};

// Runs the binary in a sub-process and returns its standard output.
StatusOr<std::string> RunProcess(const char* binary_path_and_args, bool quiet) {
  // std::system() is in the standard but pclose() can return stdout without an
  // extra file.
  std::unique_ptr<FILE, decltype(&pclose)> file_descriptor(
      popen(binary_path_and_args, "r"), pclose);
  CHECK_OR_RETURN(file_descriptor != nullptr, quiet)
      << "popen(" << binary_path_and_args << ") failure";
  std::string standard_output;
  char buffer[128];
  while (fgets(buffer, 128, file_descriptor.get()) != nullptr) {
    standard_output += buffer;
  }
  return standard_output;
}

//------------------------------------------------------------------------------

StatusOr<float> GetLibwebp2Distortion(const WP2::ArgbBuffer& reference,
                                      const WP2::ArgbBuffer& image,
                                      const TaskInput& task,
                                      WP2::MetricType metric, bool quiet) {
  float distortion[5];
  const WP2Status status =
      (task.codec_settings.quality == kQualityLossless ||
       !reference.HasTransparency())
          ? image.GetDistortion(reference, metric, distortion)
          : image.GetDistortionBlackOrWhiteBackground(reference, metric,
                                                      distortion);

  if (status == WP2_STATUS_UNSUPPORTED_FEATURE &&
      WP2FormatBpp(reference.format()) != 4) {
    // Some metrics need four channels.
    WP2::ArgbBuffer reference4(
        WP2IsPremultiplied(reference.format()) ? WP2_Argb_32 : WP2_ARGB_32);
    CHECK_OR_RETURN(reference4.ConvertFrom(reference) == WP2_STATUS_OK, quiet);
    WP2::ArgbBuffer image4(WP2IsPremultiplied(image.format()) ? WP2_Argb_32
                                                              : WP2_ARGB_32);
    CHECK_OR_RETURN(image4.ConvertFrom(image) == WP2_STATUS_OK, quiet);
    return GetLibwebp2Distortion(reference4, image4, task, metric, quiet);
  }

  CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
      << "GetDistortion(" << metric << ") failed on " << task.image_path
      << " and " << CodecName(task.codec_settings.codec) << " at effort "
      << task.codec_settings.effort << ", chroma subsampling "
      << SubsamplingToString(task.codec_settings.chroma_subsampling)
      << " and quality " << task.codec_settings.quality << ": "
      << WP2GetStatusMessage(status);
  float overall_distortion = distortion[4];

  if (metric == WP2::PSNR &&
      (task.codec_settings.quality == kQualityLossless
           ? overall_distortion != kNoDistortion
           : overall_distortion <
                 (task.codec_settings.quality > 90 ? 10 : 2))) {
    if (!quiet) {
      std::cerr << "Error: " << task.image_path
                << " was encoded or decoded with loss in "
                << CodecName(task.codec_settings.codec) << " format at effort "
                << task.codec_settings.effort << ", chroma subsampling "
                << SubsamplingToString(task.codec_settings.chroma_subsampling)
                << " and quality " << task.codec_settings.quality << " (alpha "
                << distortion[0] << "dB, R " << distortion[1] << "dB, G "
                << distortion[2] << "dB, B " << distortion[3] << "dB, overall "
                << overall_distortion << "dB)" << std::endl;
    }
    // Uncomment to dump the problematic image.
#if 0
    (void)WP2::SaveImage(reference, "/tmp/ccgen_original.png");
    (void)WP2::SaveImage(image, "/tmp/ccgen_decoded.png");
#endif
    return Status::kUnknownError;
  }
  return overall_distortion;
}

Status SaveImage(const WP2::ArgbBuffer& image, const std::string& file_path,
                 bool quiet) {
  const WP2Status status =
      WP2::SaveImage(image, file_path.c_str(), /*overwrite=*/true);
  if (status == WP2_STATUS_UNSUPPORTED_FEATURE &&
      image.format() != WP2_Argb_32 && image.format() != WP2_ARGB_32) {
    WP2::ArgbBuffer image4(WP2IsPremultiplied(image.format()) ? WP2_Argb_32
                                                              : WP2_ARGB_32);
    CHECK_OR_RETURN(image4.ConvertFrom(image) == WP2_STATUS_OK, quiet);
    return ::codec_compare_gen::SaveImage(image4, file_path, quiet);
  }

  CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
      << "SaveImage(" << file_path
      << ") failed: " << WP2GetStatusMessage(status);
  return Status::kOk;
}

StatusOr<std::string> GetBinaryDistortion(const std::string& reference_path,
                                          const WP2::ArgbBuffer& reference,
                                          const std::string& image_path,
                                          const WP2::ArgbBuffer& image,
                                          const TaskInput& task,
                                          const std::string& metric_binary_path,
                                          size_t thread_id, bool quiet) {
  CHECK_OR_RETURN(!reference_path.empty(), quiet);
  CHECK_OR_RETURN(!metric_binary_path.empty(), quiet);

  // Create a PNG file containing the original pixels of the current frame if
  // not PNG (could be a GIF with multiple frames for example).
  const bool maybeAnimated = !EndsWith(reference_path, ".png");
  std::string temp_reference_path;
  std::string_view final_reference_path = reference_path;
  if (maybeAnimated) {
    // Thread-safe file name.
    temp_reference_path =
        std::filesystem::temp_directory_path() /
        ("codec_compare_gen_reference" + std::to_string(thread_id) + ".png");
    OK_OR_RETURN(SaveImage(reference, temp_reference_path, quiet));
    final_reference_path = temp_reference_path;
  }
  const FileDeleter temp_reference_path_deleter(temp_reference_path);

  // Create a PNG file containing the decoded pixels if not done or if animated.
  std::string temp_image_path;
  std::string_view final_image_path = image_path;
  if (image_path.empty() || maybeAnimated) {
    // Thread-safe file name.
    temp_image_path =
        std::filesystem::temp_directory_path() /
        ("codec_compare_gen_image" + std::to_string(thread_id) + ".png");
    OK_OR_RETURN(SaveImage(image, temp_image_path, quiet));
    final_image_path = temp_image_path;
  }
  const FileDeleter temp_image_path_deleter(temp_image_path);

  const std::string binary_path_and_args = Escape(metric_binary_path) + " " +
                                           Escape(final_reference_path) + " " +
                                           Escape(final_image_path);
  return RunProcess(binary_path_and_args.c_str(), quiet);
}

StatusOr<float> GetLibjxlDistortion(
    const std::string& reference_path, const WP2::ArgbBuffer& reference,
    const std::string& image_path, const WP2::ArgbBuffer& image,
    const TaskInput& task, const std::string& metric_binary_folder_path,
    DistortionMetric metric, size_t thread_id, bool quiet) {
  // Metric binaries are not available: just return -1 for simplicity.
  if (metric_binary_folder_path.empty()) return -1;

  const char* metric_binary_name;
  if (metric == DistortionMetric::kLibjxlButteraugli ||
      metric == DistortionMetric::kLibjxlP3norm) {
    metric_binary_name = "butteraugli_main";
  } else if (metric == DistortionMetric::kLibjxlSsimulacra) {
    metric_binary_name = "ssimulacra_main";
  } else {
    CHECK_OR_RETURN(metric == DistortionMetric::kLibjxlSsimulacra2, quiet);
    metric_binary_name = "ssimulacra2";
  }
  const std::string metric_binary_path =
      std::filesystem::path(metric_binary_folder_path) / "libjxl" / "build" /
      "tools" / metric_binary_name;
  ASSIGN_OR_RETURN(
      std::string standard_output,
      GetBinaryDistortion(reference_path, reference, image_path, image, task,
                          metric_binary_path, thread_id, quiet));

  if (metric == DistortionMetric::kLibjxlButteraugli ||
      metric == DistortionMetric::kLibjxlP3norm) {
    static constexpr const char* kP3normToken = "3-norm:";
    const auto p3norm_token_position = standard_output.find(kP3normToken);
    CHECK_OR_RETURN(p3norm_token_position != std::string::npos, quiet)
        << "\"3-norm:\" token not found in \"" << standard_output << "\"";
    standard_output = metric == DistortionMetric::kLibjxlButteraugli
                          ? standard_output.substr(0, p3norm_token_position)
                          : standard_output.substr(p3norm_token_position +
                                                   std::strlen(kP3normToken));
  }

  return std::stof(Trim(standard_output));
}

StatusOr<float> GetDssimDistortion(const std::string& reference_path,
                                   const WP2::ArgbBuffer& reference,
                                   const std::string& image_path,
                                   const WP2::ArgbBuffer& image,
                                   const TaskInput& task,
                                   const std::string& metric_binary_folder_path,
                                   size_t thread_id, bool quiet) {
  // Metric binaries are not available: just return -1 for simplicity.
  if (metric_binary_folder_path.empty()) return -1;

  const std::string metric_binary_path =
      std::filesystem::path(metric_binary_folder_path) / "dssim" / "target" /
      "release" / "dssim";
  ASSIGN_OR_RETURN(
      const std::string standard_output,
      GetBinaryDistortion(reference_path, reference, image_path, image, task,
                          metric_binary_path, thread_id, quiet));
  return std::stof(Trim(Split(standard_output, '\t').front()));
}

StatusOr<float> GetDistortion(
    const std::string& reference_path, const WP2::ArgbBuffer& reference,
    const std::string& image_path, const WP2::ArgbBuffer& image,
    const TaskInput& task, const std::string& metric_binary_folder_path,
    DistortionMetric metric, size_t thread_id, bool quiet) {
  switch (metric) {
    case DistortionMetric::kLibwebp2Psnr:
      return GetLibwebp2Distortion(reference, image, task, WP2::PSNR, quiet);
    case DistortionMetric::kLibwebp2Ssim:
      return GetLibwebp2Distortion(reference, image, task, WP2::SSIM, quiet);
    case DistortionMetric::kLibjxlButteraugli:
    case DistortionMetric::kLibjxlSsimulacra:
    case DistortionMetric::kLibjxlSsimulacra2:
    case DistortionMetric::kLibjxlP3norm:
      return GetLibjxlDistortion(reference_path, reference, image_path, image,
                                 task, metric_binary_folder_path, metric,
                                 thread_id, quiet);
    case DistortionMetric::kDssim:
      return GetDssimDistortion(reference_path, reference, image_path, image,
                                task, metric_binary_folder_path, thread_id,
                                quiet);
  }
  return Status::kUnknownError;
}

}  // namespace

StatusOr<float> GetAverageDistortion(
    const std::string& a_path, const Image& a, const std::string& b_path,
    const Image& b, const TaskInput& task,
    const std::string& metric_binary_folder_path, DistortionMetric metric,
    size_t thread_id, bool quiet) {
  CHECK_OR_RETURN(!a.empty() && !b.empty(), quiet);
  if (a.size() == 1 && b.size() == 1) {
    return GetDistortion(a_path, a.front().pixels, b_path, b.front().pixels,
                         task, metric_binary_folder_path, metric, thread_id,
                         quiet);
  }

  const uint32_t a_duration_ms = GetDurationMs(a);
  CHECK_OR_RETURN(a_duration_ms > 0, quiet);
  CHECK_OR_RETURN(a_duration_ms == GetDurationMs(b), quiet);

  float distortion_sum = 0;
  size_t a_index = 0, b_index = 0;
  uint32_t previous_time = 0, a_time = 0, b_time = 0;  // milliseconds
  do {
    ASSIGN_OR_RETURN(
        const float distortion,
        GetDistortion(a_path, a[a_index].pixels, b_path, b[b_index].pixels,
                      task, metric_binary_folder_path, metric, thread_id,
                      quiet));

    const uint32_t next_a_time = a_time + a[a_index].duration_ms;
    const uint32_t next_b_time = b_time + b[b_index].duration_ms;
    const uint32_t current_time = std::min(next_a_time, next_b_time);
    // Weigh the distortion by frame duration.
    distortion_sum += distortion * (current_time - previous_time);

    if (current_time >= next_a_time) {
      ++a_index;
      a_time = next_a_time;
    }
    if (current_time >= next_b_time) {
      ++b_index;
      b_time = next_b_time;
    }
    previous_time = current_time;
  } while (a_index < a.size() && b_index < b.size());
  CHECK_OR_RETURN(a_index == a.size() && b_index == b.size(), quiet);
  CHECK_OR_RETURN(a_time == b_time && a_time == a_duration_ms, quiet);
  return distortion_sum / a_duration_ms;
}

StatusOr<bool> PixelEquality(const WP2::ArgbBuffer& a, const WP2::ArgbBuffer& b,
                             bool quiet) {
  CHECK_OR_RETURN(a.format() == b.format(), quiet);
  CHECK_OR_RETURN(a.width() == b.width() && a.height() == b.height(), quiet);
  for (uint32_t y = 0; y < a.height(); ++y) {
    if (!std::equal(a.GetRow8(y),
                    a.GetRow8(y) + a.width() * WP2FormatBpp(a.format()),
                    b.GetRow8(y))) {
      return false;
    }
  }
  return true;
}

StatusOr<bool> PixelEquality(const Image& a, const Image& b, bool quiet) {
  CHECK_OR_RETURN(!a.empty() && !b.empty(), quiet);
  if (a.size() == 1 && b.size() == 1) {
    return PixelEquality(a.front().pixels, b.front().pixels, quiet);
  }

  const uint32_t a_duration_ms = GetDurationMs(a);
  CHECK_OR_RETURN(a_duration_ms > 0, quiet);
  CHECK_OR_RETURN(a_duration_ms == GetDurationMs(b), quiet);

  size_t a_index = 0, b_index = 0;
  uint32_t a_time = 0, b_time = 0;  // milliseconds
  do {
    ASSIGN_OR_RETURN(
        const bool pixel_equality,
        PixelEquality(a[a_index].pixels, b[b_index].pixels, quiet));
    if (!pixel_equality) return false;

    const uint32_t next_a_time = a_time + a[a_index].duration_ms;
    const uint32_t next_b_time = b_time + b[b_index].duration_ms;
    const uint32_t current_time = std::min(next_a_time, next_b_time);
    if (current_time >= next_a_time) {
      ++a_index;
      a_time = next_a_time;
    }
    if (current_time >= next_b_time) {
      ++b_index;
      b_time = next_b_time;
    }
  } while (a_index < a.size() && b_index < b.size());
  CHECK_OR_RETURN(a_index == a.size() && b_index == b.size(), quiet);
  CHECK_OR_RETURN(a_time == b_time && a_time == a_duration_ms, quiet);
  return true;
}

#else
StatusOr<float> GetAverageDistortion(const std::string&, const Image&,
                                     const std::string&, const Image&,
                                     const TaskInput&, const std::string&,
                                     DistortionMetric, size_t, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Computing distortions requires HAS_WEBP2";
}
StatusOr<bool> PixelEquality(const Image&, const Image&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Equality check requires HAS_WEBP2";
}
#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
