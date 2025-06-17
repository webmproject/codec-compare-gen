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
#include "src/serialization.h"
#include "src/task.h"
#include "src/timer.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif  // HAS_WEBP2

namespace codec_compare_gen {

std::string CodecName(Codec codec) {
  switch (codec) {
    case Codec::kWebp:
      return "webp";
    case Codec::kWebp2:
      return "webp2";
    case Codec::kJpegXl:
      return "jpegxl";
    case Codec::kAvif:
      return "avif";
    case Codec::kAvifExp:
      return "avifexp";
    case Codec::kAvifAvm:
      return "avifavm";
    case Codec::kCombination:
      return "combination";
    case Codec::kJpegturbo:
      return "jpegturbo";
    case Codec::kJpegli:
      return "jpegli";
    case Codec::kJpegsimple:
      return "jpegsimple";
    case Codec::kJpegmoz:
      return "jpegmoz";
    case Codec::kJp2:
      return "jp2";
    case Codec::kFfv1:
      return "ffv1";
    case Codec::kBasis:
      return "basis";
    case Codec::kNumCodecs:
      break;
  }
  assert(false);
  return "unknown";
}

std::string CodecPrettyName(Codec codec, bool lossless, Subsampling subsampling,
                            int effort) {
  const std::string subsampling_str =
      (lossless && (subsampling == Subsampling::kDefault ||
                    subsampling == Subsampling::k444))
          ? ""
          : (subsampling == Subsampling::k444 ? " 4:4:4" : " 4:2:0");
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
    case Codec::kJp2:
      return "JPEG2000" + subsampling_str;  // No effort setting.
    case Codec::kFfv1:
      return "FFV1" + subsampling_str;  // No effort setting.
    case Codec::kBasis:
      return "Basis";  // No effort setting, only 4:4:4.
    case Codec::kNumCodecs:
      break;
  }
  assert(false);
  return "unknown" + subsampling_str;
}

std::string CodecVersion(Codec codec) {
  switch (codec) {
    case Codec::kWebp:
      return WebpVersion();
    case Codec::kWebp2:
      return Webp2Version();
    case Codec::kJpegXl:
      return JpegXLVersion();
    case Codec::kAvif:
      return AvifVersion();
    case Codec::kAvifExp:
      return AvifVersion() + "_exp";
    case Codec::kAvifAvm:
      return AvifVersion() + "_avm";
    case Codec::kCombination:
      return CodecCombinationVersion();
    case Codec::kJpegturbo:
      return JpegturboVersion();
    case Codec::kJpegli:
      return JpegliVersion();
    case Codec::kJpegsimple:
      return JpegsimpleVersion();
    case Codec::kJpegmoz:
      return JpegmozVersion();
    case Codec::kJp2:
      return OpenjpegVersion();
    case Codec::kFfv1:
      return Ffv1Version();
    case Codec::kBasis:
      return BasisVersion();
    case Codec::kNumCodecs:
      break;
  }
  assert(false);
  return "unknown";
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
  if (name == "jpegmoz") return Codec::kJpegmoz;
  if (name == "jp2") return Codec::kJp2;
  if (name == "ffv1") return Codec::kFfv1;
  CHECK_OR_RETURN(name == "basis", quiet) << "Unknown codec \"" << name << "\"";
  return Codec::kBasis;
}

std::vector<int> CodecLossyQualities(Codec codec) {
  switch (codec) {
    case Codec::kWebp:
      return WebpLossyQualities();
    case Codec::kWebp2:
      return Webp2LossyQualities();
    case Codec::kJpegXl:
      return JpegXLLossyQualities();
    case Codec::kAvif:
    case Codec::kAvifExp:
    case Codec::kAvifAvm:
      return AvifLossyQualities();
    case Codec::kCombination:
      return CodecCombinationLossyQualities();
    case Codec::kJpegturbo:
      return JpegturboLossyQualities();
    case Codec::kJpegli:
      return JpegliLossyQualities();
    case Codec::kJpegsimple:
      return JpegsimpleLossyQualities();
    case Codec::kJpegmoz:
      return JpegmozLossyQualities();
    case Codec::kJp2:
      return OpenjpegLossyQualities();
    case Codec::kFfv1:
      return {};
    case Codec::kBasis:
      return BasisLossyQualities();
    case Codec::kNumCodecs:
      break;
  }
  assert(false);
  return {};
}

std::string CodecExtension(Codec codec) {
  switch (codec) {
    case Codec::kWebp:
      return "webp";
    case Codec::kWebp2:
      return "wp2";
    case Codec::kJpegXl:
      return "jxl";
    case Codec::kAvif:
      return "avif";
    case Codec::kAvifExp:
      // See "MIME type registration" Annex in
      // "ISO/IEC 23008-12 3rd edition DAM 2 Low-overhead image file format"
      // https://www.mpeg.org/wp-content/uploads/mpeg_meetings/149_Geneva/w24745.zip
      return "hmg";
    case Codec::kAvifAvm:
      return "avmf";
    case Codec::kCombination:
      return "comb";
    case Codec::kJpegturbo:
      return "turbo.jpg";
    case Codec::kJpegli:
      return "li.jpg";
    case Codec::kJpegsimple:
      return "s.jpg";
    case Codec::kJpegmoz:
      return "moz.jpg";
    case Codec::kJp2:
      return "jp2";  // Matches OPJ_CODEC_JP2 used in codec_openjpeg.cc.
    case Codec::kFfv1:
      return "ffv1";
    case Codec::kBasis:
      return "basis";
    case Codec::kNumCodecs:
      break;
  }
  assert(false);
  return "unknown";
}

bool CodecIsSupportedByBrowsers(Codec codec) {
  switch (codec) {
    case Codec::kWebp:
    case Codec::kAvif:
    case Codec::kJpegturbo:
    case Codec::kJpegli:
    case Codec::kJpegsimple:
    case Codec::kJpegmoz:
      return true;
    case Codec::kWebp2:
    case Codec::kJpegXl:
    case Codec::kAvifExp:
    case Codec::kAvifAvm:
    case Codec::kCombination:
    case Codec::kJp2:
    case Codec::kFfv1:
    case Codec::kBasis:
      return false;
    case Codec::kNumCodecs:
      break;
  }
  assert(false);
  return false;
}

#if defined(HAS_WEBP2)

namespace {

bool CodecSupportsBitDepth(Codec codec, uint32_t d) {
  switch (codec) {
    case Codec::kWebp:
      return d == 8;
    case Codec::kWebp2:
      return d == 8 || d == 10;  // 10 useless here.
    case Codec::kJpegXl:
      return d == 8 || d == 16;
    case Codec::kAvif:
    case Codec::kAvifExp:
    case Codec::kAvifAvm:
      return d == 8 || d == 10 || d == 12;  // 10/12 useless here.
    case Codec::kCombination:
      return d == 8;
    case Codec::kJpegturbo:
    case Codec::kJpegli:
    case Codec::kJpegsimple:
    case Codec::kJpegmoz:
      return d == 8;
    case Codec::kJp2:
      return d == 8 || d == 16;
    case Codec::kFfv1:
      return d == 8;
    case Codec::kBasis:
      return d == 8;
    case Codec::kNumCodecs:
      break;
  }
  assert(false);
  return false;
}

// Returns the 8-bit format layout required by the API of the given codec.
WP2SampleFormat CodecToNeededFormat(Codec codec, bool has_transparency) {
  switch (codec) {
    case Codec::kWebp:
      return WebPPictureFormat();
    case Codec::kWebp2:
      return WP2_ARGB_32;  // Even for opaque images.
    case Codec::kJpegXl:
      return has_transparency ? WP2_RGBA_32 : WP2_RGB_24;
    case Codec::kAvif:
    case Codec::kAvifExp:
    case Codec::kAvifAvm:
      return has_transparency ? WP2_ARGB_32 : WP2_RGB_24;
    case Codec::kCombination:
      return WP2_ARGB_32;  // Even for opaque images.
    case Codec::kJpegturbo:
    case Codec::kJpegli:
    case Codec::kJpegsimple:
    case Codec::kJpegmoz:
      return WP2_RGB_24;
    case Codec::kJp2:
      return has_transparency ? WP2_RGBA_32 : WP2_RGB_24;
    case Codec::kFfv1:
      return WP2_BGRA_32;
    case Codec::kBasis:
      return has_transparency ? WP2_RGBA_32 : WP2_RGB_24;
    case Codec::kNumCodecs:
      break;
  }
  assert(false);
  return WP2_FORMAT_NUM;
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
    // Ditch alpha if the image is opaque.
    ASSIGN_OR_RETURN(original_image,
                     CloneAs(original_image, needed_format, quiet));
  }
  if (WP2Formatbpc(original_image.front().pixels.format()) == 16 &&
      !CodecSupportsBitDepth(input.codec_settings.codec, 16) &&
      input.codec_settings.quality == kQualityLossless) {
    // The codec does not support 16-bit images. Consider the frames to be 8-bit
    // and twice as large. The compression rate is likely terrible.
    ASSIGN_OR_RETURN(original_image, SpreadTo8bit(original_image, quiet));
  }
  CHECK_OR_RETURN(CodecSupportsBitDepth(
                      input.codec_settings.codec,
                      WP2Formatbpc(original_image.front().pixels.format())),
                  quiet);

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
      : input.codec_settings.codec == Codec::kJp2        ? &EncodeOpenjpeg
      : input.codec_settings.codec == Codec::kFfv1       ? &EncodeFfv1
      : input.codec_settings.codec == Codec::kBasis      ? &EncodeBasis
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
      : input.codec_settings.codec == Codec::kJp2        ? &DecodeOpenjpeg
      : input.codec_settings.codec == Codec::kFfv1       ? &DecodeFfv1
      : input.codec_settings.codec == Codec::kBasis      ? &DecodeBasis
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
