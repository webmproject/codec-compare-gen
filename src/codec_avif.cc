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
    image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_IDENTITY;
  }
  if (subsampling == Subsampling::kDefault ||
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
  ASSIGN_OR_RETURN(rgb_image.format,
                   WP2SampleFormatToAvifRGBFormat(wp2_image.format()));
  rgb_image.alphaPremultiplied = WP2IsPremultiplied(wp2_image.format());
  rgb_image.pixels = const_cast<uint8_t*>(wp2_image.GetRow8(0));
  rgb_image.rowBytes = wp2_image.stride();
  CHECK_OR_RETURN(avifImageRGBToYUV(image.get(), &rgb_image) == AVIF_RESULT_OK,
                  quiet)
      << "avifImageRGBToYUV() failed";
  return image;
}

StatusOr<WP2::ArgbBuffer> AvifImageToArgbBuffer(const avifImage& image,
                                                bool quiet) {
  WP2::ArgbBuffer wp2_image(image.alphaPlane ? WP2_ARGB_32 : WP2_RGB_24);
  CHECK_OR_RETURN(wp2_image.Resize(image.width, image.height) == WP2_STATUS_OK,
                  quiet);

  avifRGBImage rgb_image;
  avifRGBImageSetDefaults(&rgb_image, &image);
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

}  // namespace

StatusOr<WP2::Data> EncodeAvif(const TaskInput& input,
                               const WP2::ArgbBuffer& original_image,
                               bool quiet) {
  const bool lossless = input.codec_settings.quality == kQualityLossless;
  ASSIGN_OR_RETURN(
      avif::ImagePtr image,
      ArgbBufferToAvifImage(original_image, lossless,
                            input.codec_settings.chroma_subsampling, quiet));

  avif::EncoderPtr encoder(avifEncoderCreate());
  CHECK_OR_RETURN(encoder != nullptr, quiet) << "avifEncoderCreate() failed";
  encoder->speed = input.codec_settings.effort;  // Simpler not to reverse.
  encoder->quality =
      lossless ? AVIF_QUALITY_LOSSLESS : input.codec_settings.quality;
  encoder->qualityAlpha = encoder->quality;

  RwData encoded;
  CHECK_OR_RETURN(
      avifEncoderWrite(encoder.get(), image.get(), &encoded) == AVIF_RESULT_OK,
      quiet)
      << "avifEncoderWrite() failed: " << encoder->diag.error;

  WP2::Data encoded_image;
  std::swap(encoded_image.bytes, encoded.data);
  std::swap(encoded_image.size, encoded.size);
  return encoded_image;
}

StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeAvif(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  avif::ImagePtr image(avifImageCreateEmpty());
  CHECK_OR_RETURN(image != nullptr, quiet) << "avifImageCreateEmpty() failed";
  avif::DecoderPtr decoder(avifDecoderCreate());
  CHECK_OR_RETURN(decoder != nullptr, quiet) << "avifDecoderCreate() failed";

  CHECK_OR_RETURN(
      avifDecoderReadMemory(decoder.get(), image.get(), encoded_image.bytes,
                            encoded_image.size) == AVIF_RESULT_OK,
      quiet)
      << "avifDecoderReadMemory() failed: " << decoder->diag.error;

  // Measure color conversion time.
  std::pair<WP2::ArgbBuffer, double> image_and_color_conversion_duration;
  const Timer color_conversion_duration;
  ASSIGN_OR_RETURN(image_and_color_conversion_duration.first,
                   AvifImageToArgbBuffer(*image, quiet));
  image_and_color_conversion_duration.second =
      color_conversion_duration.seconds();
  return image_and_color_conversion_duration;
}

#else
StatusOr<WP2::Data> EncodeAvif(const TaskInput&, const WP2::ArgbBuffer&,
                               bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_AVIF";
}
StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeAvif(const TaskInput&,
                                                        const WP2::Data&,
                                                        bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_AVIF";
}
#endif  // HAS_AVIF

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
