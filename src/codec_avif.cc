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

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/frame.h"
#include "src/serialization.h"
#include "src/task.h"
#include "src/timer.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

#if defined(HAS_AVIF)
#include "avif/avif.h"
#include "avif/avif_cxx.h"
#endif

namespace codec_compare_gen {

std::string AvifVersion() {
#if defined(HAS_AVIF)
  return std::to_string(AVIF_VERSION_MAJOR) + "." +
         std::to_string(AVIF_VERSION_MINOR) + "." +
         std::to_string(AVIF_VERSION_PATCH);
#else
  return "n/a";
#endif
}

std::vector<int> AvifLossyQualities() {
  std::vector<int> qualities(64);
  for (int i = 0; i < qualities.size(); ++i) {
    // Reverse avifQualityToQuantizer():
    //   quantizer = ((100 - quality) * 63 + 50) / 100;
    qualities[i] = ((63 - i) * 100 + 63 / 2) / 63;
  }
  return qualities;  // [0:63] (63 is lossless but in YUV so RGB is lossy).
}

#if defined(HAS_WEBP2)

#if defined(HAS_AVIF)

namespace {

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
                                               bool lossless,
                                               Subsampling subsampling,
                                               bool quiet) {
  avif::ImagePtr image(avifImageCreate(wp2_image.width(), wp2_image.height(),
                                       WP2Formatbpc(wp2_image.format()),
                                       AVIF_PIXEL_FORMAT_YUV444));
  CHECK_OR_RETURN(image != nullptr, quiet) << "avifImageCreate() failed";
  if (lossless) {
    image->colorPrimaries = AVIF_COLOR_PRIMARIES_UNSPECIFIED;
    image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED;
    // AVIF_MATRIX_COEFFICIENTS_YCGCO_RE
    image->matrixCoefficients = (avifMatrixCoefficients)16;
    CHECK_OR_RETURN(WP2Formatbpc(wp2_image.format()) == 8, quiet)
        << "Unexpected format " << wp2_image.format();
    image->depth = 10;
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

StatusOr<WP2::Data> EncodeAvifImpl(const TaskInput& input,
                                   const Image& original_image,
                                   bool minimized_image_box, bool avm,
                                   bool quiet) {
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

  RwData encoded;
  if (original_image.size() == 1) {
    ASSIGN_OR_RETURN(
        avif::ImagePtr yuv,
        ArgbBufferToAvifImage(original_image.front().pixels, lossless,
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
                           frame.pixels, lossless,
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

StatusOr<std::pair<Image, double>> DecodeAvifImpl(
    const TaskInput& input, const WP2::Data& encoded_image, bool avm,
    bool quiet) {
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

}  // namespace

StatusOr<WP2::Data> EncodeAvif(const TaskInput& input,
                               const Image& original_image, bool quiet) {
  // Requires libavif to be built with AVIF_ENABLE_EXPERIMENTAL_YCGCO_R for
  // lossless.
  return EncodeAvifImpl(input, original_image, /*minimized_image_box=*/false,
                        /*avm=*/false, quiet);
}

StatusOr<WP2::Data> EncodeSlimAvif(const TaskInput& input,
                                   const Image& original_image, bool quiet) {
  // Requires libavif to be built with AVIF_ENABLE_EXPERIMENTAL_YCGCO_R for
  // lossless and AVIF_ENABLE_EXPERIMENTAL_MINI.
  return EncodeAvifImpl(input, original_image, /*minimized_image_box=*/true,
                        /*avm=*/false, quiet);
}

StatusOr<WP2::Data> EncodeSlimAvifAvm(const TaskInput& input,
                                      const Image& original_image, bool quiet) {
  // Requires libavif to be built with AVIF_ENABLE_EXPERIMENTAL_YCGCO_R for
  // lossless, AVIF_ENABLE_EXPERIMENTAL_MINI and AVIF_CODEC_AVM.
  return EncodeAvifImpl(input, original_image, /*minimized_image_box=*/true,
                        /*avm=*/true, quiet);
}

StatusOr<std::pair<Image, double>> DecodeAvif(const TaskInput& input,
                                              const WP2::Data& encoded_image,
                                              bool quiet) {
  return DecodeAvifImpl(input, encoded_image, /*avm=*/false, quiet);
}

StatusOr<std::pair<Image, double>> DecodeAvifAvm(const TaskInput& input,
                                                 const WP2::Data& encoded_image,
                                                 bool quiet) {
  return DecodeAvifImpl(input, encoded_image, /*avm=*/true, quiet);
}

#else
StatusOr<WP2::Data> EncodeAvif(const TaskInput&, const Image&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_AVIF";
}
StatusOr<WP2::Data> EncodeSlimAvif(const TaskInput&, const Image&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_AVIF";
}
StatusOr<WP2::Data> EncodeSlimAvifAvm(const TaskInput&, const Image&,
                                      bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_AVIF";
}
StatusOr<std::pair<Image, double>> DecodeAvif(const TaskInput&,
                                              const WP2::Data&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_AVIF";
}
StatusOr<std::pair<Image, double>> DecodeAvifAvm(const TaskInput&,
                                                 const WP2::Data&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_AVIF";
}
#endif  // HAS_AVIF

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
