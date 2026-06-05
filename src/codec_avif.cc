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

#include "src/codec_avif.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/codec.h"
#include "src/frame.h"
#include "src/serialization.h"
#include "src/task.h"
#include "src/timer.h"
#include "src/wp2/base.h"

#if defined(HAS_AVIF)
#include "avif/avif.h"
#include "avif/avif_cxx.h"
#endif

namespace codec_compare_gen {
namespace {

std::string AvifPrettyName(bool lossless, Subsampling subsampling, int effort) {
  return "AVIF s" + std::to_string(effort) +
         SubsamplingToPrettyString(lossless, subsampling);
}

std::string AvifSsimPrettyName(bool lossless, Subsampling subsampling,
                               int effort) {
  return "AVIF tune=SSIM s" + std::to_string(effort) +
         SubsamplingToPrettyString(lossless, subsampling);
}

std::string AvifIqPrettyName(bool lossless, Subsampling subsampling,
                             int effort) {
  return "AVIF tune=IQ s" + std::to_string(effort) +
         SubsamplingToPrettyString(lossless, subsampling);
}

std::string AvifExpPrettyName(bool lossless, Subsampling subsampling,
                              int effort) {
  return "AVIFmini s" + std::to_string(effort) +
         SubsamplingToPrettyString(lossless, subsampling);
}

std::string AvifAvmPrettyName(bool lossless, Subsampling subsampling,
                              int effort) {
  return "AVIFminiAVM s" + std::to_string(effort) +
         SubsamplingToPrettyString(lossless, subsampling);
}

std::string AvifVersion() {
#if defined(HAS_AVIF)
  return std::to_string(AVIF_VERSION_MAJOR) + "." +
         std::to_string(AVIF_VERSION_MINOR) + "." +
         std::to_string(AVIF_VERSION_PATCH);
#else
  return "n/a";
#endif
}

std::string AvifSsimVersion() { return AvifVersion() + "_tunessim"; }
std::string AvifIqVersion() { return AvifVersion() + "_tuneiq"; }
std::string AvifExpVersion() { return AvifVersion() + "_exp"; }
std::string AvifAvmVersion() { return AvifVersion() + "_avm"; }

std::vector<int> AvifLossyQualities() {
  std::vector<int> qualities(64);
  for (int i = 0; i < qualities.size(); ++i) {
    qualities[i] = ((63 - i) * 100 + 63 / 2) / 63;
  }
  std::reverse(qualities.begin(), qualities.end());
  return qualities;
}

std::vector<int> AvifIqLossyQualities() {
  std::vector<int> qualities = AvifLossyQualities();
  assert(!qualities.empty() && qualities.back() == 100);
  qualities.pop_back();
  return qualities;
}

#if defined(HAS_AVIF)

StatusOr<avifRGBFormat> WP2SampleFormatToAvifRGBFormat(WP2SampleFormat format) {
  if (format == WP2_Argb_32) return AVIF_RGB_FORMAT_ARGB;
  if (format == WP2_ARGB_32) return AVIF_RGB_FORMAT_ARGB;
  if (format == WP2_rgbA_32) return AVIF_RGB_FORMAT_RGBA;
  if (format == WP2_RGBA_32) return AVIF_RGB_FORMAT_RGBA;
  if (format == WP2_bgrA_32) return AVIF_RGB_FORMAT_BGRA;
  if (format == WP2_BGRA_32) return AVIF_RGB_FORMAT_BGRA;
  if (format == WP2_RGB_24) return AVIF_RGB_FORMAT_RGB;
  if (format == WP2_BGR_24) return AVIF_RGB_FORMAT_BGR;
  return codec_compare_gen::Status::kUnknownError;
}

StatusOr<avif::ImagePtr> ArgbBufferToAvifImage(const WP2::ArgbBuffer& wp2_image,
                                               bool lossless, bool ycgco_re,
                                               Subsampling subsampling,
                                               bool quiet) {
  avif::ImagePtr image(avifImageCreate(wp2_image.width(), wp2_image.height(),
                                       WP2Formatbpc(wp2_image.format()),
                                       AVIF_PIXEL_FORMAT_YUV444));
  CHECK_OR_RETURN(image != nullptr, quiet) << "avifImageCreate() failed";
  if (lossless) {
    image->colorPrimaries = AVIF_COLOR_PRIMARIES_UNSPECIFIED;
    image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED;
    if (ycgco_re) {
      image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_YCGCO_RE;
      image->depth = 10;
    } else {
      image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_IDENTITY;
    }
    CHECK_OR_RETURN(WP2Formatbpc(wp2_image.format()) == 8, quiet)
        << "Unexpected format " << wp2_image.format();
    CHECK_OR_RETURN(subsampling == Subsampling::kDefault ||
                        subsampling == Subsampling::k444,
                    quiet)
        << "AVIF does not support chroma subsampling "
        << SubsamplingToString(subsampling) << " for lossless encodings";
    image->yuvFormat = AVIF_PIXEL_FORMAT_YUV444;
  } else if (subsampling == Subsampling::kDefault ||
             subsampling == Subsampling::k420) {
    image->yuvFormat = AVIF_PIXEL_FORMAT_YUV420;
  } else {
    CHECK_OR_RETURN(subsampling == Subsampling::k444, quiet)
        << "AVIF does not support chroma subsampling "
        << SubsamplingToString(subsampling);
    image->yuvFormat = AVIF_PIXEL_FORMAT_YUV444;
  }
  avifRGBImage rgb_image;
  avifRGBImageSetDefaults(&rgb_image, image.get());
  if (lossless) {
    rgb_image.depth = 8;
  }
  ASSIGN_OR_RETURN(rgb_image.format,
                   WP2SampleFormatToAvifRGBFormat(wp2_image.format()));
  rgb_image.alphaPremultiplied = WP2IsPremultiplied(wp2_image.format());
  rgb_image.pixels = const_cast<uint8_t*>(wp2_image.GetRow8(0));
  rgb_image.rowBytes = wp2_image.stride();
  const avifResult result = avifImageRGBToYUV(image.get(), &rgb_image);
  CHECK_OR_RETURN(result == AVIF_RESULT_OK, quiet)
      << "avifImageRGBToYUV() failed: " << result;
  return image;
}

StatusOr<WP2::ArgbBuffer> AvifImageToArgbBuffer(const avifImage& image,
                                                bool quiet) {
  WP2::ArgbBuffer wp2_image(image.alphaPlane ? WP2_ARGB_32 : WP2_RGB_24);
  CHECK_OR_RETURN(wp2_image.Resize(image.width, image.height) == WP2_STATUS_OK,
                  quiet);

  avifRGBImage rgb_image;
  avifRGBImageSetDefaults(&rgb_image, &image);
  if (image.matrixCoefficients == (avifMatrixCoefficients)16) {
    CHECK_OR_RETURN(image.depth == 10, quiet)
        << "Unexpected depth " << image.depth;
    rgb_image.depth = 8;
  }
  ASSIGN_OR_RETURN(rgb_image.format,
                   WP2SampleFormatToAvifRGBFormat(wp2_image.format()));
  rgb_image.alphaPremultiplied = WP2IsPremultiplied(wp2_image.format());
  rgb_image.pixels = const_cast<uint8_t*>(wp2_image.GetRow8(0));
  rgb_image.rowBytes = wp2_image.stride();
  CHECK_OR_RETURN(avifImageYUVToRGB(&image, &rgb_image) == AVIF_RESULT_OK,
                  quiet)
      << "avifImageYUVToRGB() failed";
  return wp2_image;
}

class RwData : public avifRWData {
 public:
  RwData() : avifRWData{nullptr, 0} {}
  ~RwData() { avifRWDataFree(this); }
};

StatusOr<WP2::Data> EncodeAvif(const TaskInput& input,
                               const Image& original_image,
                               bool minimized_image_box, bool ycgco_re,
                               const char* tune, bool avm, bool quiet) {
  const bool lossless = input.codec_settings.quality == kQualityLossless;

  avif::EncoderPtr encoder(avifEncoderCreate());
  CHECK_OR_RETURN(encoder != nullptr, quiet) << "avifEncoderCreate() failed";
  encoder->speed = input.codec_settings.effort;  // Simpler not to reverse.
  encoder->quality =
      lossless ? AVIF_QUALITY_LOSSLESS : input.codec_settings.quality;
  encoder->qualityAlpha = encoder->quality;
  encoder->codecChoice = avm ? AVIF_CODEC_CHOICE_AVM : AVIF_CODEC_CHOICE_AUTO;
  encoder->headerFormat = minimized_image_box
                              ? (avifHeaderFormat)1  // AVIF_HEADER_REDUCED
                              : AVIF_HEADER_FULL;
  CHECK_OR_RETURN(avifEncoderSetCodecSpecificOption(encoder.get(), "tune",
                                                    tune) == AVIF_RESULT_OK,
                  quiet);

  RwData encoded;
  if (original_image.size() == 1) {
    ASSIGN_OR_RETURN(
        avif::ImagePtr yuv,
        ArgbBufferToAvifImage(original_image.front().pixels, lossless, ycgco_re,
                              input.codec_settings.chroma_subsampling, quiet));
    CHECK_OR_RETURN(
        avifEncoderWrite(encoder.get(), yuv.get(), &encoded) == AVIF_RESULT_OK,
        quiet)
        << "avifEncoderWrite() failed: " << encoder->diag.error;
  } else {
    encoder->timescale = 1000;  // milliseconds
    for (const Frame& frame : original_image) {
      ASSIGN_OR_RETURN(avif::ImagePtr yuv,
                       ArgbBufferToAvifImage(
                           frame.pixels, lossless, ycgco_re,
                           input.codec_settings.chroma_subsampling, quiet));
      CHECK_OR_RETURN(
          avifEncoderAddImage(encoder.get(), yuv.get(), frame.duration_ms,
                              AVIF_ADD_IMAGE_FLAG_NONE) == AVIF_RESULT_OK,
          quiet)
          << "avifEncoderAddImage() failed: " << encoder->diag.error;
    }
    CHECK_OR_RETURN(
        avifEncoderFinish(encoder.get(), &encoded) == AVIF_RESULT_OK, quiet)
        << "avifEncoderFinish() failed: " << encoder->diag.error;
  }

  WP2::Data encoded_image;
  std::swap(encoded_image.bytes, encoded.data);
  std::swap(encoded_image.size, encoded.size);
  return encoded_image;
}

StatusOr<std::pair<Image, double>> DecodeAvif(const TaskInput& input,
                                              const WP2::Data& encoded_image,
                                              bool avm, bool quiet) {
  avif::DecoderPtr decoder(avifDecoderCreate());
  CHECK_OR_RETURN(decoder != nullptr, quiet);
  decoder->codecChoice = avm ? AVIF_CODEC_CHOICE_AVM : AVIF_CODEC_CHOICE_AUTO;

  CHECK_OR_RETURN(avifDecoderSetIOMemory(decoder.get(), encoded_image.bytes,
                                         encoded_image.size) == AVIF_RESULT_OK,
                  quiet);
  CHECK_OR_RETURN(avifDecoderParse(decoder.get()) == AVIF_RESULT_OK, quiet)
      << "avifDecoderParse() failed: " << decoder->diag.error;
  if (decoder->imageCount > 1) {
    CHECK_OR_RETURN(decoder->timescale == 1000, quiet) << decoder->timescale;
  }

  Image image;
  image.reserve(decoder->imageCount);
  avifResult result;
  double color_conversion_duration = 0;
  while ((result = avifDecoderNextImage(decoder.get())) == AVIF_RESULT_OK) {
    const Timer timer;
    ASSIGN_OR_RETURN(WP2::ArgbBuffer buffer,
                     AvifImageToArgbBuffer(*decoder->image, quiet));
    color_conversion_duration += timer.seconds();
    const uint32_t duration_ms =
        decoder->imageCount == 1
            ? 0
            : static_cast<uint32_t>(decoder->imageTiming.durationInTimescales);
    image.emplace_back(std::move(buffer), duration_ms);
  }
  return std::pair<Image, double>(std::move(image), color_conversion_duration);
}
#else
StatusOr<WP2::Data> EncodeAvif(const TaskInput&, const Image&, bool, bool,
                               const char*, bool, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_AVIF";
}
StatusOr<std::pair<Image, double>> DecodeAvif(const TaskInput&,
                                              const WP2::Data&, bool,
                                              bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_AVIF";
}
#endif  // HAS_AVIF

StatusOr<WP2::Data> EncodeAvifRegular(const TaskInput& input,
                                      const Image& original_image, bool quiet) {
  return EncodeAvif(input, original_image, /*minimized_image_box=*/false,
                    /*ycgco_re=*/false, /*tune=*/nullptr, /*avm=*/false, quiet);
}
StatusOr<WP2::Data> EncodeAvifSsim(const TaskInput& input,
                                   const Image& original_image, bool quiet) {
  return EncodeAvif(input, original_image, /*minimized_image_box=*/false,
                    /*ycgco_re=*/false, /*tune=*/"ssim", /*avm=*/false, quiet);
}
StatusOr<WP2::Data> EncodeAvifIq(const TaskInput& input,
                                 const Image& original_image, bool quiet) {
  return EncodeAvif(input, original_image, /*minimized_image_box=*/false,
                    /*ycgco_re=*/false, /*tune=*/"iq", /*avm=*/false, quiet);
}
StatusOr<std::pair<Image, double>> DecodeAvifRegularOrExp(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  return DecodeAvif(input, encoded_image, /*avm=*/false, quiet);
}
StatusOr<WP2::Data> EncodeAvifExp(const TaskInput& input,
                                  const Image& original_image, bool quiet) {
  return EncodeAvif(input, original_image, /*minimized_image_box=*/true,
                    /*ycgco_re=*/true, /*tune=*/nullptr, /*avm=*/false, quiet);
}
StatusOr<WP2::Data> EncodeAvifAvm(const TaskInput& input,
                                  const Image& original_image, bool quiet) {
  return EncodeAvif(input, original_image, /*minimized_image_box=*/true,
                    /*ycgco_re=*/true, /*tune=*/nullptr, /*avm=*/true, quiet);
}
StatusOr<std::pair<Image, double>> DecodeAvifAvm(const TaskInput& input,
                                                 const WP2::Data& encoded_image,
                                                 bool quiet) {
  return DecodeAvif(input, encoded_image, /*avm=*/true, quiet);
}

}  // namespace

CodecMetadata GetAvifMetadata() {
  return CodecMetadata{
      "avif",
      AvifPrettyName,
      AvifVersion,
      AvifLossyQualities,
      "avif",
      /*is_supported_by_browsers=*/true,
      /*supports_16bit=*/false,
      /*opaque_format=*/WP2_RGB_24,
      /*transparent_format=*/WP2_ARGB_32,
      EncodeAvifRegular,
      DecodeAvifRegularOrExp,
  };
}

CodecMetadata GetAvifSsimMetadata() {
  return CodecMetadata{
      "avifssim",
      AvifSsimPrettyName,
      AvifSsimVersion,
      AvifLossyQualities,
      "ssim.avif",
      /*is_supported_by_browsers=*/true,
      /*supports_16bit=*/false,
      /*opaque_format=*/WP2_RGB_24,
      /*transparent_format=*/WP2_ARGB_32,
      EncodeAvifSsim,
      DecodeAvifRegularOrExp,
  };
}

CodecMetadata GetAvifIqMetadata() {
  return CodecMetadata{
      "avifiq",
      AvifIqPrettyName,
      AvifIqVersion,
      AvifIqLossyQualities,
      "iq.avif",
      /*is_supported_by_browsers=*/true,
      /*supports_16bit=*/false,
      /*opaque_format=*/WP2_RGB_24,
      /*transparent_format=*/WP2_ARGB_32,
      EncodeAvifIq,
      DecodeAvifRegularOrExp,
  };
}

CodecMetadata GetAvifExpMetadata() {
  return CodecMetadata{
      "avifexp",
      AvifExpPrettyName,
      AvifExpVersion,
      AvifLossyQualities,
      "hmg",
      /*is_supported_by_browsers=*/false,
      /*supports_16bit=*/false,
      /*opaque_format=*/WP2_RGB_24,
      /*transparent_format=*/WP2_ARGB_32,
      EncodeAvifExp,
      DecodeAvifRegularOrExp,
  };
}

CodecMetadata GetAvifAvmMetadata() {
  return CodecMetadata{
      "avifavm",
      AvifAvmPrettyName,
      AvifAvmVersion,
      AvifLossyQualities,
      "avmf",
      /*is_supported_by_browsers=*/false,
      /*supports_16bit=*/false,
      /*opaque_format=*/WP2_RGB_24,
      /*transparent_format=*/WP2_ARGB_32,
      EncodeAvifAvm,
      DecodeAvifAvm,
  };
}

}  // namespace codec_compare_gen
