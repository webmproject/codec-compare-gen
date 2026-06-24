// Copyright 2026 Google LLC
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

#include "src/codec_jpegzune.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/codec.h"
#include "src/frame.h"
#include "src/serialization.h"
#include "src/task.h"
#include "src/wp2/base.h"

#if defined(HAS_JPEGZUNE)
#include "bindings/jpeg/jpegzune_rs.h"
#endif

namespace codec_compare_gen {
namespace {

std::string VersionToString(int version) {
  return std::to_string((version >> 16) & 0xff) + "." +
         std::to_string((version >> 8) & 0xff) + "." +
         std::to_string(version & 0xff);
}

std::string JpegzunePrettyName(bool lossless, Subsampling subsampling,
                               int effort) {
  assert(!lossless);
  assert(subsampling == Subsampling::k444 ||
         subsampling == Subsampling::kDefault);
  assert(effort == 0);
  return "image-rs JPEG (zune dec)";
}

std::string JpegzuneVersion() {
#if defined(HAS_JPEGZUNE)
  return "image-rs" + VersionToString(ccgen_imagejpeg_encoder_version()) +
         "/zune-jpeg" + VersionToString(ccgen_zunejpeg_version());
#else
  return "n/a";
#endif
}

std::vector<int> JpegzuneEfforts() { return {}; }

std::vector<int> JpegzuneLossyQualities() {
  std::vector<int> qualities(100);
  std::iota(qualities.begin(), qualities.end(), 1);
  return qualities;
}

#if defined(HAS_JPEGZUNE)

StatusOr<WP2::Data> EncodeJpegimage(const TaskInput& input,
                                    const Image& original_image, bool quiet) {
  CHECK_OR_RETURN(original_image.size() == 1, quiet);
  const WP2::ArgbBuffer& pixels = original_image.front().pixels;
  CHECK_OR_RETURN(input.codec_settings.effort == 0, quiet);
  CHECK_OR_RETURN(pixels.format() == WP2_RGB_24, quiet);

  CHECK_OR_RETURN(
      input.codec_settings.chroma_subsampling == Subsampling::kDefault ||
          input.codec_settings.chroma_subsampling == Subsampling::k444,
      quiet)
      << "The image crate JPEG encoder does not support chroma subsampling "
      << SubsamplingToString(input.codec_settings.chroma_subsampling);

  uint8_t* compressed_image = nullptr;
  size_t compressed_num_bytes = 0;

  int result = ccgen_imagejpeg_encode444(
      pixels.GetRow8(0), pixels.width(), pixels.height(), pixels.stride(),
      input.codec_settings.quality, &compressed_image, &compressed_num_bytes);
  CHECK_OR_RETURN(result == 1, quiet) << "ccgen_imagejpeg_encode444() failed";

  WP2::Data data;
  CHECK_OR_RETURN(
      data.CopyFrom(compressed_image, compressed_num_bytes) == WP2_STATUS_OK,
      quiet);
  ccgen_imagejpeg_free_buffer(compressed_image, compressed_num_bytes);
  return data;
}

StatusOr<std::pair<Image, double>> DecodeJpegzune(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  size_t width = 0;
  size_t height = 0;
  uint8_t* decoded_bytes = nullptr;

  int result = ccgen_zunejpeg_decode(encoded_image.bytes, encoded_image.size,
                                     &width, &height, &decoded_bytes);
  CHECK_OR_RETURN(result == 1, quiet) << "ccgen_zunejpeg_decode() failed";

  Image image;
  image.reserve(1);
  image.emplace_back(WP2::ArgbBuffer(WP2_RGB_24), /*duration_ms=*/0);
  CHECK_OR_RETURN(image.back().pixels.Resize(static_cast<uint32_t>(width),
                                             static_cast<uint32_t>(height)) ==
                      WP2_STATUS_OK,
                  quiet);

  WP2::ArgbBuffer& pixels = image.back().pixels;
  for (size_t y = 0; y < height; ++y) {
    std::memcpy(pixels.GetRow8(y), decoded_bytes + y * width * 3, width * 3);
  }

  ccgen_imagejpeg_free_buffer(decoded_bytes, width * height * 3);

  return std::pair<Image, double>(std::move(image), 0);
}

#else
StatusOr<WP2::Data> EncodeJpegimage(const TaskInput&, const Image&,
                                    bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_JPEGZUNE";
}
StatusOr<std::pair<Image, double>> DecodeJpegzune(const TaskInput&,
                                                  const WP2::Data&,
                                                  bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_JPEGZUNE";
}
#endif  // HAS_JPEGZUNE

}  // namespace

CodecMetadata GetJpegzuneMetadata() {
  return CodecMetadata{
      "jpegzune",
      JpegzunePrettyName,
      JpegzuneVersion,
      " -DCCGEN_ENABLE_AVIF=OFF -DCCGEN_ENABLE_JPEG=OFF "
      "-DCCGEN_ENABLE_JPEGZUNE=ON",
      JpegzuneEfforts,
      JpegzuneLossyQualities,
      "zune.jpg",
      /*is_supported_by_browsers=*/false,
      /*supports_16bit=*/false,
      /*opaque_format=*/WP2_RGB_24,
      /*transparent_format=*/WP2_FORMAT_NUM,  // No alpha support.
      EncodeJpegimage,
      DecodeJpegzune,
  };
}

namespace {
std::string JpegturboEncJpegzuneDecPrettyName(bool lossless,
                                              Subsampling subsampling,
                                              int effort) {
  return GetCodecMetadata(Codec::kJpegturbo)
             .pretty_name(lossless, subsampling, effort) +
         " (zune dec)";
}

std::string JpegturboEncJpegzuneDecVersion() {
#if defined(HAS_JPEGZUNE)
  return GetCodecMetadata(Codec::kJpegturbo).version() + "/zune-jpeg" +
         VersionToString(ccgen_zunejpeg_version());
#else
  return "n/a";
#endif
}
}  // namespace

CodecMetadata GetJpegturboEncJpegzuneDecMetadata() {
  return CodecMetadata{
      "jpegturboencjpegzunedec",
      JpegturboEncJpegzuneDecPrettyName,
      JpegturboEncJpegzuneDecVersion,
      " -DCCGEN_ENABLE_AVIF=OFF -DCCGEN_ENABLE_JPEG=ON"
      " -DCCGEN_ENABLE_JPEGZUNE=ON",
      GetCodecMetadata(Codec::kJpegturbo).efforts,
      GetCodecMetadata(Codec::kJpegturbo).lossy_qualities,
      "turbo.jpg",
      GetCodecMetadata(Codec::kJpegturbo).is_supported_by_browsers,
      GetCodecMetadata(Codec::kJpegturbo).supports_16bit,
      GetCodecMetadata(Codec::kJpegturbo).opaque_format,
      GetCodecMetadata(Codec::kJpegturbo).transparent_format,
      GetCodecMetadata(Codec::kJpegturbo).encode,
      DecodeJpegzune,
  };
}

}  // namespace codec_compare_gen
