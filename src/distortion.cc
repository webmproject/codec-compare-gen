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

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "src/base.h"
#include "src/codec.h"
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
                 (task.codec_settings.quality > 90 ? 20 : 2))) {
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

  // Create a PNG file containing the decoded pixels if not done.
  std::string temp_image_path;
  if (image_path.empty()) {
    // Thread-safe file name.
    temp_image_path =
        std::filesystem::temp_directory_path() /
        ("codec_compare_gen_image" + std::to_string(thread_id) + ".png");
    OK_OR_RETURN(SaveImage(image, temp_image_path, quiet));
  }
  const FileDeleter temp_image_path_deleter(temp_image_path);
  const std::string& final_image_path =
      image_path.empty() ? temp_image_path : image_path;

  const std::string binary_path_and_args =
      metric_binary_path + " " + reference_path + " " + final_image_path;

  return RunProcess(binary_path_and_args.c_str(), quiet);
}

StatusOr<float> GetLibjxlDistortion(
    const std::string& reference_path, const WP2::ArgbBuffer& reference,
    const std::string& image_path, const WP2::ArgbBuffer& image,
    const TaskInput& task, const std::string& metric_binary_folder_path,
    DistortionMetric metric, size_t thread_id, bool quiet) {
  // Metric binaries are not available: just return 0 for simplicity.
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
  // Metric binaries are not available: just return 0 for simplicity.
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

}  // namespace

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

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
