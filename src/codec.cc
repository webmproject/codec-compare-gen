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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/codec_avif.h"
#include "src/codec_combination.h"
#include "src/codec_jpegli.h"
#include "src/codec_jpegmoz.h"
#include "src/codec_jpegsimple.h"
#include "src/codec_jpegturbo.h"
#include "src/codec_jpegxl.h"
#include "src/codec_webp.h"
#include "src/codec_webp2.h"
#include "src/distortion.h"
#include "src/frame.h"
#include "src/framework.h"
#include "src/serialization.h"
#include "src/task.h"
#include "src/timer.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif  // HAS_WEBP2

namespace codec_compare_gen {

std::string CodecName(Codec codec) {
  return codec == Codec::kWebp          ? "webp"
         : codec == Codec::kWebp2       ? "webp2"
         : codec == Codec::kJpegXl      ? "jpegxl"
         : codec == Codec::kAvif        ? "avif"
         : codec == Codec::kAvifExp     ? "avifexp"
         : codec == Codec::kAvifAvm     ? "avifavm"
         : codec == Codec::kCombination ? "combination"
         : codec == Codec::kJpegturbo   ? "jpegturbo"
         : codec == Codec::kJpegli      ? "jpegli"
         : codec == Codec::kJpegsimple  ? "jpegsimple"
                                        : "jpegmoz";
}

std::string CodecPrettyName(Codec codec, bool lossless, Subsampling subsampling,
                            int effort) {
  const std::string subsampling_str =
      (lossless && (subsampling == Subsampling::kDefault ||
                    subsampling == Subsampling::k444))
          ? ""
          : (" " + SubsamplingToString(subsampling));
  switch (codec) {
    case Codec::kWebp:
      return (lossless ? "WebP z" : "WebP m") + std::to_string(effort) +
             subsampling_str;
    case Codec::kWebp2:
      return "WebP2 e" + std::to_string(effort) + subsampling_str;
    case Codec::kJpegXl:
      return "JPEG XL e" + std::to_string(effort);  // Only 4:4:4.
    case Codec::kAvif:
      return "AVIF s" + std::to_string(effort) + subsampling_str;
    case Codec::kAvifExp:
      return "AVIFmini" + std::string(lossless ? "YCgCo" : "") +
             (" s" + std::to_string(effort)) + subsampling_str;
    case Codec::kAvifAvm:
      // YCgCo-Re is also used with AVM but save column width by omitting it.
      return "AVIFminiAVM s" + std::to_string(effort) + subsampling_str;
    case Codec::kCombination:
      return "combination e" + std::to_string(effort) + subsampling_str;
    case Codec::kJpegturbo:
      return "TurboJPEG" + subsampling_str;  // No effort setting.
    case Codec::kJpegli:
      return "Jpegli" + subsampling_str;  // No effort setting.
    case Codec::kJpegsimple:
      return "SimpleJPEG m" + std::to_string(effort) + subsampling_str;
    case Codec::kJpegmoz:
      return "MozJPEG" + subsampling_str;  // No effort setting.
    default:
      return "unknown" + subsampling_str;
  }
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
  } else if (codec == Codec::kAvifExp) {
    return AvifVersion() + "_exp";
  } else if (codec == Codec::kAvifAvm) {
    return AvifVersion() + "_avm";
  } else if (codec == Codec::kCombination) {
    return CodecCombinationVersion();
  } else if (codec == Codec::kJpegturbo) {
    return JpegturboVersion();
  } else if (codec == Codec::kJpegli) {
    return JpegliVersion();
  } else if (codec == Codec::kJpegsimple) {
    return JpegsimpleVersion();
  } else {
    assert(codec == Codec::kJpegmoz);
    return JpegmozVersion();
  }
}

StatusOr<Codec> CodecFromName(const std::string& name, bool quiet) {
  if (name == "webp") return Codec::kWebp;
  if (name == "webp2") return Codec::kWebp2;
  if (name == "jpegxl") return Codec::kJpegXl;
  if (name == "avif") return Codec::kAvif;
  if (name == "avifexp") return Codec::kAvifExp;
  if (name == "avifavm") return Codec::kAvifAvm;
  if (name == "combination") return Codec::kCombination;
  if (name == "jpegturbo") return Codec::kJpegturbo;
  if (name == "jpegli") return Codec::kJpegli;
  if (name == "jpegsimple") return Codec::kJpegsimple;
  CHECK_OR_RETURN(name == "jpegmoz", quiet)
      << "Unknown codec \"" << name << "\"";
  return Codec::kJpegmoz;
}

std::vector<int> CodecLossyQualities(Codec codec) {
  if (codec == Codec::kWebp) return WebpLossyQualities();
  if (codec == Codec::kWebp2) return Webp2LossyQualities();
  if (codec == Codec::kJpegXl) return JpegXLLossyQualities();
  if (codec == Codec::kAvif) return AvifLossyQualities();
  if (codec == Codec::kAvifExp) return AvifLossyQualities();
  if (codec == Codec::kAvifAvm) return AvifLossyQualities();
  if (codec == Codec::kCombination) return CodecCombinationLossyQualities();
  if (codec == Codec::kJpegturbo) return JpegturboLossyQualities();
  if (codec == Codec::kJpegli) return JpegliLossyQualities();
  if (codec == Codec::kJpegsimple) return JpegsimpleLossyQualities();
  assert(codec == Codec::kJpegmoz);
  return JpegmozLossyQualities();
}

std::string CodecExtension(Codec codec) {
  return codec == Codec::kWebp          ? "webp"
         : codec == Codec::kWebp2       ? "wp2"
         : codec == Codec::kJpegXl      ? "jxl"
         : codec == Codec::kAvif        ? "avif"
         : codec == Codec::kAvifExp     ? "avif"
         : codec == Codec::kAvifAvm     ? "avif"
         : codec == Codec::kCombination ? "comb"
         : codec == Codec::kJpegturbo   ? "turbo.jpg"
         : codec == Codec::kJpegli      ? "li.jpg"
         : codec == Codec::kJpegsimple  ? "s.jpg"
         : codec == Codec::kJpegmoz     ? "moz.jpg"
                                        : "unknown";
}

bool CodecIsSupportedByBrowsers(Codec codec) {
  return codec == Codec::kWebp || codec == Codec::kAvif ||
         codec == Codec::kJpegturbo || codec == Codec::kJpegli ||
         codec == Codec::kJpegsimple || codec == Codec::kJpegmoz;
}

#if defined(HAS_WEBP2)

namespace {

// Returns the 8-bit format layout required by the API of the given codec.
WP2SampleFormat CodecToNeededFormat(Codec codec, bool has_transparency) {
  if (codec == Codec::kWebp) {
    return WebPPictureFormat();
  }
  if (codec == Codec::kJpegXl) {
    return has_transparency ? WP2_RGBA_32 : WP2_RGB_24;
  }
  if (codec == Codec::kAvif || codec == Codec::kAvifExp ||
      codec == Codec::kAvifAvm) {
    return has_transparency ? WP2_ARGB_32 : WP2_RGB_24;
  }
  if (codec == Codec::kJpegturbo || codec == Codec::kJpegli ||
      codec == Codec::kJpegsimple || codec == Codec::kJpegmoz) {
    return WP2_RGB_24;
  }
  // Other formats support this layout even for opaque images.
  return WP2_ARGB_32;
}

// Variants of AVIF.
StatusOr<WP2::Data> EncodeAvifRegular(const TaskInput& input,
                                      const Image& original_image, bool quiet) {
  return EncodeAvif(input, original_image, /*minimized_image_box=*/false,
                    /*ycgco_re=*/false, /*avm=*/false, quiet);
}
StatusOr<std::pair<Image, double>> DecodeAvifRegularOrExp(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  return DecodeAvif(input, encoded_image, /*avm=*/false, quiet);
}
StatusOr<WP2::Data> EncodeAvifExp(const TaskInput& input,
                                  const Image& original_image, bool quiet) {
  return EncodeAvif(input, original_image, /*minimized_image_box=*/true,
                    /*ycgco_re=*/true, /*avm=*/false, quiet);
}
StatusOr<WP2::Data> EncodeAvifAvm(const TaskInput& input,
                                  const Image& original_image, bool quiet) {
  return EncodeAvif(input, original_image, /*minimized_image_box=*/true,
                    /*ycgco_re=*/true, /*avm=*/true, quiet);
}
StatusOr<std::pair<Image, double>> DecodeAvifAvm(const TaskInput& input,
                                                 const WP2::Data& encoded_image,
                                                 bool quiet) {
  return DecodeAvif(input, encoded_image, /*avm=*/true, quiet);
}

}  // namespace

StatusOr<TaskOutput> EncodeDecode(const TaskInput& input,
                                  const std::string& metric_binary_folder_path,
                                  size_t thread_id, EncodeMode encode_mode,
                                  bool quiet) {
  TaskOutput task;
  task.task_input = input;

  const WP2SampleFormat initial_format = CodecToNeededFormat(
      input.codec_settings.codec, /*has_transparency=*/true);
  ASSIGN_OR_RETURN(Image original_image,
                   ReadStillImageOrAnimation(input.image_path.c_str(),
                                             initial_format, quiet));

  bool has_transparency = false;
  for (const Frame& frame : original_image) {
    has_transparency |= frame.pixels.HasTransparency();
  }
  WP2SampleFormat needed_format =
      CodecToNeededFormat(input.codec_settings.codec, has_transparency);
  if (initial_format != needed_format) {
    needed_format = WP2FormatAtbpc(
        needed_format, WP2Formatbpc(original_image.front().pixels.format()));
    CHECK_OR_RETURN(needed_format != WP2_FORMAT_NUM, quiet);
    // Ditch alpha because the image is opaque.
    ASSIGN_OR_RETURN(original_image,
                     CloneAs(original_image, needed_format, quiet));
  }
  if (WP2Formatbpc(original_image.front().pixels.format()) == 16 &&
      input.codec_settings.codec != Codec::kJpegXl &&
      input.codec_settings.quality == kQualityLossless) {
    // The codec does not support 16-bit images. Consider the frames to be 8-bit
    // and twice as large. The compression rate is likely terrible.
    ASSIGN_OR_RETURN(original_image, SpreadTo8bit(original_image, quiet));
  }

  auto encode_func =
      input.codec_settings.codec == Codec::kWebp      ? &EncodeWebp
      : input.codec_settings.codec == Codec::kWebp2   ? &EncodeWebp2
      : input.codec_settings.codec == Codec::kJpegXl  ? &EncodeJxl
      : input.codec_settings.codec == Codec::kAvif    ? &EncodeAvifRegular
      : input.codec_settings.codec == Codec::kAvifExp ? &EncodeAvifExp
      : input.codec_settings.codec == Codec::kAvifAvm ? &EncodeAvifAvm
      : input.codec_settings.codec == Codec::kCombination
          ? &EncodeCodecCombination
      : input.codec_settings.codec == Codec::kJpegturbo  ? &EncodeJpegturbo
      : input.codec_settings.codec == Codec::kJpegli     ? &EncodeJpegli
      : input.codec_settings.codec == Codec::kJpegsimple ? &EncodeJpegsimple
      : input.codec_settings.codec == Codec::kJpegmoz    ? &EncodeJpegmoz
                                                         : nullptr;
  auto decode_func =
      input.codec_settings.codec == Codec::kWebp      ? &DecodeWebp
      : input.codec_settings.codec == Codec::kWebp2   ? &DecodeWebp2
      : input.codec_settings.codec == Codec::kJpegXl  ? &DecodeJxl
      : input.codec_settings.codec == Codec::kAvif    ? &DecodeAvifRegularOrExp
      : input.codec_settings.codec == Codec::kAvifExp ? &DecodeAvifRegularOrExp
      : input.codec_settings.codec == Codec::kAvifAvm ? &DecodeAvifAvm
      : input.codec_settings.codec == Codec::kCombination
          ? &DecodeCodecCombination
      : input.codec_settings.codec == Codec::kJpegturbo  ? &DecodeJpegturbo
      : input.codec_settings.codec == Codec::kJpegli     ? &DecodeJpegli
      : input.codec_settings.codec == Codec::kJpegsimple ? &DecodeJpegsimple
      : input.codec_settings.codec == Codec::kJpegmoz    ? &DecodeJpegmoz
                                                         : nullptr;

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
    CHECK_OR_RETURN(encode_func != nullptr, quiet);
    ASSIGN_OR_RETURN(encoded_image, encode_func(input, original_image, quiet));
  }
  task.encoding_duration = encoding_duration.seconds();
  task.image_width = original_image.front().pixels.width();
  task.image_height = original_image.front().pixels.height();
  task.bit_depth = WP2Formatbpc(original_image.front().pixels.format());
  task.num_frames = static_cast<uint32_t>(original_image.size());
  task.encoded_size = encoded_image.size;

  const Timer decoding_duration;
  Image decoded_image;
  CHECK_OR_RETURN(decode_func != nullptr, quiet);
  {
    ASSIGN_OR_RETURN(auto image_and_color_conversion_duration,
                     decode_func(input, encoded_image, quiet));
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
        << input.image_path << " encoded with "
        << CodecName(task.task_input.codec_settings.codec)
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

#else

StatusOr<TaskOutput> EncodeDecode(const TaskInput&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Reading images requires HAS_WEBP2";
}

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
