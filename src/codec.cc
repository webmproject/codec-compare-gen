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
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "imageio/anim_image_dec.h"
#include "src/base.h"
#include "src/codec_avif.h"
#include "src/codec_avif_libheif.h"
#include "src/codec_basis.h"
#include "src/codec_combination.h"
#include "src/codec_ffv1.h"
#include "src/codec_jpegli.h"
#include "src/codec_jpegmoz.h"
#include "src/codec_jpegsimple.h"
#include "src/codec_jpegturbo.h"
#include "src/codec_jpegxl.h"
#include "src/codec_openjpeg.h"
#include "src/codec_webp.h"
#include "src/codec_webp2.h"
#include "src/distortion.h"
#include "src/frame.h"
#include "src/framework.h"
#include "src/task.h"
#include "src/timer.h"
#include "src/wp2/base.h"

namespace codec_compare_gen {

CodecMetadata GetCodecMetadata(Codec codec) {
  switch (codec) {
    case Codec::kWebp:
      return GetWebpMetadata();
    case Codec::kWebp2:
      return GetWebp2Metadata();
    case Codec::kJpegXl:
      return GetJpegXlMetadata();
    case Codec::kAvif:
      return GetAvifMetadata();
    case Codec::kAvifSsim:
      return GetAvifSsimMetadata();
    case Codec::kAvifIq:
      return GetAvifIqMetadata();
    case Codec::kAvifExp:
      return GetAvifExpMetadata();
    case Codec::kAvifAvm:
      return GetAvifAvmMetadata();
    case Codec::kAvifLibheif:
      return GetAvifLibheifMetadata();
    case Codec::kCombination:
      return GetCombinationMetadata();
    case Codec::kJpegturbo:
      return GetJpegturboMetadata();
    case Codec::kJpegli:
      return GetJpegliMetadata();
    case Codec::kJpegsimple:
      return GetJpegsimpleMetadata();
    case Codec::kJpegmoz:
      return GetJpegmozMetadata();
    case Codec::kJp2:
      return GetJp2Metadata();
    case Codec::kFfv1:
      return GetFfv1Metadata();
    case Codec::kBasis:
      return GetBasisMetadata();
    case Codec::kNumCodecs:
      break;
  }
  assert(false);
  return CodecMetadata();
}

StatusOr<Codec> CodecFromName(const std::string& name, bool quiet) {
  for (int c = 0; c < static_cast<int>(Codec::kNumCodecs); ++c) {
    if (GetCodecMetadata(static_cast<Codec>(c)).name == name) {
      return static_cast<Codec>(c);
    }
  }
  CHECK_OR_RETURN(false, quiet) << "Unknown codec \"" << name << "\"";
  return Status::kUnknownError;
}

std::string SubsamplingToPrettyString(bool lossless, Subsampling subsampling) {
  return (lossless && (subsampling == Subsampling::kDefault ||
                       subsampling == Subsampling::k444))
             ? ""
             : (subsampling == Subsampling::k444 ? " 4:4:4" : " 4:2:0");
}

StatusOr<TaskOutput> EncodeDecode(const TaskInput& input,
                                  const std::string& metric_binary_folder_path,
                                  size_t thread_id, EncodeMode encode_mode,
                                  bool quiet) {
  TaskOutput task;
  task.task_input = input;

  const CodecMetadata& codec = GetCodecMetadata(input.codec_settings.codec);
  const bool supports_transparency = codec.transparent_format != WP2_FORMAT_NUM;
  const WP2SampleFormat initial_format =
      supports_transparency ? codec.transparent_format : codec.opaque_format;
  ASSIGN_OR_RETURN(Image original_image,
                   ReadStillImageOrAnimation(input.image_path.c_str(),
                                             initial_format, quiet));

  bool has_transparency = false;
  for (const Frame& frame : original_image) {
    has_transparency |= frame.pixels.HasTransparency();
  }
  WP2SampleFormat needed_format =
      has_transparency ? codec.transparent_format : codec.opaque_format;
  if (initial_format != needed_format) {
    needed_format = WP2FormatAtbpc(
        needed_format, WP2Formatbpc(original_image.front().pixels.format()));
    CHECK_OR_RETURN(needed_format != WP2_FORMAT_NUM, quiet);
    // Ditch alpha if the image is opaque.
    ASSIGN_OR_RETURN(original_image,
                     CloneAs(original_image, needed_format, quiet));
  }
  if (WP2Formatbpc(original_image.front().pixels.format()) == 16 &&
      !codec.supports_16bit &&
      input.codec_settings.quality == kQualityLossless) {
    // The codec does not support 16-bit images. Consider the frames to be 8-bit
    // and twice as large. The compression rate is likely terrible.
    ASSIGN_OR_RETURN(original_image, SpreadTo8bit(original_image, quiet));
  }
  CHECK_OR_RETURN(
      WP2Formatbpc(original_image.front().pixels.format()) == 8 ||
          (WP2Formatbpc(original_image.front().pixels.format()) == 16 &&
           codec.supports_16bit),
      quiet);

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
  } else {
    CHECK_OR_RETURN(codec.encode != nullptr, quiet);
    ASSIGN_OR_RETURN(encoded_image, codec.encode(input, original_image, quiet));
  }
  task.encoding_duration = encoding_duration.seconds();
  task.image_width = original_image.front().pixels.width();
  task.image_height = original_image.front().pixels.height();
  task.bit_depth = WP2Formatbpc(original_image.front().pixels.format());
  task.num_frames = static_cast<uint32_t>(original_image.size());
  task.encoded_size = encoded_image.size;

  const Timer decoding_duration;
  Image decoded_image;
  CHECK_OR_RETURN(codec.decode != nullptr, quiet);
  {
    ASSIGN_OR_RETURN(auto image_and_color_conversion_duration,
                     codec.decode(input, encoded_image, quiet));
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
    if (!codec.is_supported_by_browsers) {
      // Also write a PNG or WebP of the decoded image to disk for convenience.
      // Keep the PNG extension for the simplicity of the whole pipeline.
      decoded_path = input.encoded_path + ".png";
      OK_OR_RETURN(WriteStillImageOrAnimation(decoded_image,
                                              decoded_path.c_str(), quiet));
    }
  }

  ASSIGN_OR_RETURN(const bool pixel_equality,
                   PixelEquality(original_image, decoded_image, quiet));
  if (task.task_input.codec_settings.quality == kQualityLossless &&
      !pixel_equality) {
    ASSIGN_OR_RETURN(const float psnr,
                     GetAverageDistortion(
                         input.image_path, original_image, decoded_path,
                         decoded_image, input, metric_binary_folder_path,
                         DistortionMetric::kLibwebp2Psnr, thread_id, quiet));
    CHECK_OR_RETURN(false, quiet)
        << input.image_path << " encoded with " << codec.name
        << " was not decoded losslessly (PSNR " << psnr << "dB)";
  }

  if (pixel_equality) {
    std::fill(task.distortions, task.distortions + kNumDistortionMetrics,
              kNoDistortion);
  } else {
    for (size_t m = 0; m < kNumDistortionMetrics; ++m) {
      ASSIGN_OR_RETURN(task.distortions[m],
                       GetAverageDistortion(
                           input.image_path, original_image, decoded_path,
                           decoded_image, input, metric_binary_folder_path,
                           static_cast<DistortionMetric>(m), thread_id, quiet));
    }
  }
  return task;
}

StatusOr<std::vector<uint8_t>> Encode(const uint8_t* argb, uint32_t width,
                                      uint32_t height, Codec codec,
                                      Subsampling chroma_subsampling,
                                      int effort, int quality, bool quiet) {
  TaskInput input;
  input.codec_settings = {codec, chroma_subsampling, effort, quality};

  WP2::ArgbBuffer buffer(WP2_ARGB_32);
  OK_WP2_OR_RETURN(buffer.Import(WP2_ARGB_32, width, height, argb, width * 4),
                   quiet);

  Image original_image;
  original_image.emplace_back(std::move(buffer), /*duration_ms=*/0);

  const bool supports_transparency =
      GetCodecMetadata(codec).transparent_format != WP2_FORMAT_NUM;
  const WP2SampleFormat needed_format =
      supports_transparency && original_image.front().pixels.HasTransparency()
          ? GetCodecMetadata(codec).transparent_format
          : GetCodecMetadata(codec).opaque_format;
  if (original_image.front().pixels.format() != needed_format) {
    ASSIGN_OR_RETURN(original_image,
                     CloneAs(original_image, needed_format, quiet));
  }

  CHECK_OR_RETURN(GetCodecMetadata(codec).encode != nullptr, quiet);
  ASSIGN_OR_RETURN(WP2::Data encoded_image, GetCodecMetadata(codec).encode(
                                                input, original_image, quiet));

  return std::vector<uint8_t>(encoded_image.bytes,
                              encoded_image.bytes + encoded_image.size);
}

StatusOr<std::vector<uint8_t>> DecodeToArgb(const uint8_t* encoded_image,
                                            size_t encoded_size,
                                            uint32_t* width, uint32_t* height,
                                            bool quiet) {
  WP2::ArgbBuffer buffer(WP2_ARGB_32);
  WP2::ImageReader reader(encoded_image, encoded_size, &buffer);
  bool is_last;
  uint32_t duration_ms;
  OK_WP2_OR_RETURN(reader.ReadFrame(&is_last, &duration_ms), quiet);

  *width = buffer.width();
  *height = buffer.height();
  std::vector<uint8_t> output(buffer.width() * buffer.height() * 4);
  for (uint32_t y = 0; y < buffer.height(); ++y) {
    std::memcpy(&output[y * buffer.width() * 4], buffer.GetRow8(y),
                buffer.width() * 4);
  }
  return output;
}

}  // namespace codec_compare_gen
