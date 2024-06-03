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

#include "src/codec.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/codec_avif.h"
#include "src/codec_combination.h"
#include "src/codec_jpegxl.h"
#include "src/codec_webp.h"
#include "src/codec_webp2.h"
#include "src/distortion.h"
#include "src/framework.h"
#include "src/task.h"
#include "src/timer.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/imageio/file_format.h"
#include "third_party/libwebp2/imageio/image_dec.h"
#include "third_party/libwebp2/imageio/image_enc.h"
#include "third_party/libwebp2/imageio/imageio_util.h"
#include "third_party/libwebp2/src/wp2/base.h"
#endif  // HAS_WEBP2

namespace codec_compare_gen {

std::string CodecName(Codec codec) {
  return codec == Codec::kWebp     ? "webp"
         : codec == Codec::kWebp2  ? "webp2"
         : codec == Codec::kJpegXl ? "jpegxl"
         : codec == Codec::kAvif   ? "avif"
                                   : "combination";
}

std::string CodecVersion(Codec codec) {
  if (codec == Codec::kWebp) {
    return WebpVersion();
  } else if (codec == Codec::kWebp2) {
    return Webp2Version();
  } else if (codec == Codec::kJpegXl) {
    return JpegXLVersion();
  } else if (codec == Codec::kAvif) {
    return AvifVersion();
  } else {
    assert(codec == Codec::kCombination);
    return CodecCombinationVersion();
  }
}

StatusOr<Codec> CodecFromName(const std::string& name, bool quiet) {
  if (name == "webp") return Codec::kWebp;
  if (name == "webp2") return Codec::kWebp2;
  if (name == "jpegxl") return Codec::kJpegXl;
  if (name == "avif") return Codec::kAvif;
  CHECK_OR_RETURN(name == "combination", quiet)
      << "Unknown codec \"" << name << "\"";
  return Codec::kCombination;
}

std::vector<int> CodecLossyQualities(Codec codec) {
  if (codec == Codec::kWebp) return WebpLossyQualities();
  if (codec == Codec::kWebp2) return Webp2LossyQualities();
  if (codec == Codec::kJpegXl) return JpegXLLossyQualities();
  if (codec == Codec::kAvif) return AvifLossyQualities();
  assert(codec == Codec::kCombination);
  return CodecCombinationLossyQualities();
}

std::string CodecExtension(Codec codec) {
  return codec == Codec::kWebp     ? "webp"
         : codec == Codec::kWebp2  ? "wp2"
         : codec == Codec::kJpegXl ? "jxl"
         : codec == Codec::kAvif   ? "avif"
                                   : "unknown";
}

bool CodecIsSupportedByBrowsers(Codec codec) {
  return codec == Codec::kWebp || codec == Codec::kAvif;
}

#if defined(HAS_WEBP2)

namespace {

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

}  // namespace

StatusOr<TaskOutput> EncodeDecode(const TaskInput& input,
                                  const std::string& metric_binary_folder_path,
                                  size_t thread_id, EncodeMode encode_mode,
                                  bool quiet) {
  TaskOutput task;
  task.task_input = input;

  // Reuse libwebp2's wrapper for simplicity.
  // TODO(yguyon): Check PNG metadata reading capabilities.
  WP2::ArgbBuffer original_image(WP2_ARGB_32);
  const WP2Status status =
      WP2::ReadImage(input.image_path.c_str(), &original_image,
                     /*file_size=*/nullptr, WP2::FileFormat::AUTO,
                     quiet ? WP2::LogLevel::QUIET : WP2::LogLevel::VERBOSE);
  CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
      << "Got " << WP2GetStatusMessage(status) << " when reading "
      << input.image_path;

  // TODO(yguyon): Read all frames of GIF.

  const WP2SampleFormat needed_format =
      input.codec_settings.codec == Codec::kJpegXl
          ? (original_image.HasTransparency() ? WP2_RGBA_32 : WP2_RGB_24)
      : input.codec_settings.codec == Codec::kAvif
          ? (original_image.HasTransparency() ? WP2_ARGB_32 : WP2_RGB_24)
          : WP2_ARGB_32;
  if (original_image.format() != needed_format) {
    WP2::ArgbBuffer image(needed_format);
    CHECK_OR_RETURN(image.ConvertFrom(original_image) == WP2_STATUS_OK, quiet);
    original_image = std::move(image);
  }

  original_image.metadata_.Clear();

  const Timer encoding_duration;
  WP2::Data encoded_image;
  if (encode_mode == EncodeMode::kLoadFromDisk) {
    CHECK_OR_RETURN(!task.task_input.encoded_path.empty(), quiet);
    std::ifstream file{task.task_input.encoded_path, std::ios::binary};
    CHECK_OR_RETURN(file.good(), quiet);
    auto length{std::filesystem::file_size(task.task_input.encoded_path)};
    CHECK_OR_RETURN(encoded_image.Resize(length, false) == WP2_STATUS_OK,
                    quiet);
    file.read(reinterpret_cast<char*>(encoded_image.bytes),
              static_cast<long>(length));
  } else if (input.codec_settings.codec == Codec::kWebp) {
    ASSIGN_OR_RETURN(encoded_image, EncodeWebp(input, original_image, quiet));
  } else if (input.codec_settings.codec == Codec::kWebp2) {
    ASSIGN_OR_RETURN(encoded_image, EncodeWebp2(input, original_image, quiet));
  } else if (input.codec_settings.codec == Codec::kJpegXl) {
    ASSIGN_OR_RETURN(encoded_image, EncodeJxl(input, original_image, quiet));
  } else if (input.codec_settings.codec == Codec::kAvif) {
    ASSIGN_OR_RETURN(encoded_image, EncodeAvif(input, original_image, quiet));
  } else {
    assert(input.codec_settings.codec == Codec::kCombination);
    ASSIGN_OR_RETURN(encoded_image,
                     EncodeCodecCombination(input, original_image, quiet));
  }
  task.encoding_duration = encoding_duration.seconds();
  task.image_width = original_image.width();
  task.image_height = original_image.height();
  task.encoded_size = encoded_image.size;

  const Timer decoding_duration;
  WP2::ArgbBuffer decoded_image;
  if (input.codec_settings.codec == Codec::kWebp) {
    ASSIGN_OR_RETURN(auto image_and_color_conversion_duration,
                     DecodeWebp(input, encoded_image, quiet));
    decoded_image = std::move(image_and_color_conversion_duration.first);
    task.decoding_color_conversion_duration =
        image_and_color_conversion_duration.second;
  } else if (input.codec_settings.codec == Codec::kWebp2) {
    ASSIGN_OR_RETURN(auto image_and_color_conversion_duration,
                     DecodeWebp2(input, encoded_image, quiet));
    decoded_image = std::move(image_and_color_conversion_duration.first);
    task.decoding_color_conversion_duration =
        image_and_color_conversion_duration.second;
  } else if (input.codec_settings.codec == Codec::kJpegXl) {
    ASSIGN_OR_RETURN(auto image_and_color_conversion_duration,
                     DecodeJxl(input, encoded_image, quiet));
    decoded_image = std::move(image_and_color_conversion_duration.first);
    task.decoding_color_conversion_duration =
        image_and_color_conversion_duration.second;
  } else if (input.codec_settings.codec == Codec::kAvif) {
    ASSIGN_OR_RETURN(auto image_and_color_conversion_duration,
                     DecodeAvif(input, encoded_image, quiet));
    decoded_image = std::move(image_and_color_conversion_duration.first);
    task.decoding_color_conversion_duration =
        image_and_color_conversion_duration.second;
  } else {
    assert(input.codec_settings.codec == Codec::kCombination);
    ASSIGN_OR_RETURN(auto image_and_color_conversion_duration,
                     DecodeCodecCombination(input, encoded_image, quiet));
    decoded_image = std::move(image_and_color_conversion_duration.first);
    task.decoding_color_conversion_duration =
        image_and_color_conversion_duration.second;
  }
  task.decoding_duration = decoding_duration.seconds();

  std::string decoded_path;
  if (encode_mode == EncodeMode::kEncodeAndSaveToDisk) {
    CHECK_OR_RETURN(!input.encoded_path.empty(), quiet);
    std::ofstream(input.encoded_path, std::ios::binary)
        .write(reinterpret_cast<char*>(encoded_image.bytes),
               encoded_image.size);

    // Some image formats are not supported by all major browsers.
    if (!CodecIsSupportedByBrowsers(input.codec_settings.codec)) {
      // Also write a PNG of the decoded image to disk for convenience.
      decoded_path = input.encoded_path + ".png";
      OK_OR_RETURN(
          ::codec_compare_gen::SaveImage(decoded_image, decoded_path, quiet));
    }
  }

  for (size_t m = 0; m < kNumDistortionMetrics; ++m) {
    ASSIGN_OR_RETURN(
        task.distortions[m],
        GetDistortion(input.image_path, original_image, decoded_path,
                      decoded_image, input, metric_binary_folder_path,
                      static_cast<DistortionMetric>(m), thread_id, quiet));

    static_assert(static_cast<size_t>(DistortionMetric::kLibwebp2Psnr) == 0);
    if (m == static_cast<int>(DistortionMetric::kLibwebp2Psnr) &&
        task.distortions[m] == kNoDistortion) {
      // The first metric PSNR said there is no loss. Skip the other metrics.
      std::fill(task.distortions + 1, task.distortions + kNumDistortionMetrics,
                kNoDistortion);
      break;
    }
  }
  return task;
}

#else

StatusOr<TaskOutput> EncodeDecode(const TaskInput&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Reading images requires HAS_WEBP2";
}

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
