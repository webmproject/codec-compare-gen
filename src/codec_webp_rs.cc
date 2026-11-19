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

#include "src/codec_webp_rs.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/codec.h"
#include "src/frame.h"
#include "src/task.h"
#include "src/timer.h"
#include "src/wp2/base.h"

#if defined(HAS_WEBPRS)
#include "webp_rs/decode_prefix.h"
#include "webp_rs/encode_prefix.h"
#endif

namespace codec_compare_gen {
namespace {

std::string VersionToString(int version) {
  return std::to_string((version >> 16) & 0xff) + "." +
         std::to_string((version >> 8) & 0xff) + "." +
         std::to_string(version & 0xff);
}

std::string WebpRsPrettyName(bool lossless, Subsampling subsampling,
                             int effort) {
  return (lossless ? "libwebp-rs z" : "libwebp-rs m") + std::to_string(effort) +
         SubsamplingToPrettyString(lossless, subsampling);
}

std::string WebpRsVersion() {
#if defined(HAS_WEBPRS)
  if (R_WebPGetEncoderVersion() == R_WebPGetDecoderVersion()) {
    return "libwebp-rs " + VersionToString(R_WebPGetEncoderVersion());
  }
  return "libwebp-rs " + VersionToString(R_WebPGetEncoderVersion()) + "/" +
         VersionToString(R_WebPGetDecoderVersion());
#else
  return "n/a";
#endif
}

WP2SampleFormat WebPPictureFormat() {
  const uint32_t is_little_endian = 1;
  return reinterpret_cast<const uint8_t*>(&is_little_endian)[0] ? WP2_BGRA_32
                                                                : WP2_ARGB_32;
}

#if defined(HAS_WEBPRS)

// Returns a WebPPicture that points to the given ArgbBuffer.
StatusOr<WebPPicture> ArgbBufferToWebPPicture(WP2::ArgbBuffer& buffer,
                                              bool quiet) {
  WebPPicture picture = {};
  CHECK_OR_RETURN(R_WebPPictureInit(&picture), quiet);
  picture.use_argb = 1;
  picture.width = static_cast<int>(buffer.width());
  picture.height = static_cast<int>(buffer.height());
  // Avoid WebPPictureAlloc() and a copy.
  CHECK_OR_RETURN(buffer.format() == WebPPictureFormat(), quiet);
  picture.argb =
      reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(buffer.GetRow8(0)));
  picture.argb_stride =
      static_cast<int>(buffer.stride()) / WP2FormatBpp(buffer.format());
  return picture;
}

// WebPWriterFunction implementation.
int WriterFunction(const uint8_t* data, size_t data_size,
                   const WebPPicture* picture) {
  WP2::Data& bytes =
      *reinterpret_cast<WP2::Data*>(const_cast<void*>(picture->custom_ptr));
  return bytes.Append(data, data_size) == WP2_STATUS_OK ? 1 : 0;
}

StatusOr<WP2::Data> EncodeWebpRs(const TaskInput& input,
                                 const Image& original_image, bool quiet) {
  const bool lossless = input.codec_settings.quality == kQualityLossless;
  const Subsampling subsampling = input.codec_settings.chroma_subsampling;
  if (lossless) {
    CHECK_OR_RETURN(subsampling == Subsampling::kDefault ||
                        subsampling == Subsampling::k444,
                    quiet)
        << "WebP only supports lossless 4:4:4 (no chroma subsampling)";
  } else {
    CHECK_OR_RETURN(subsampling == Subsampling::kDefault ||
                        subsampling == Subsampling::k420,
                    quiet)
        << "WebP only supports lossy 4:2:0 (chroma subsampling)";
  }

  WP2::Data data;
  WP2::DataWriter writer(&data);
  WebPConfig config;
  CHECK_OR_RETURN(R_WebPConfigInit(&config), quiet)
      << "WebPConfigInit() failed";
  if (lossless) {
    CHECK_OR_RETURN(R_WebPConfigLosslessPreset(
                        &config, /*level=*/input.codec_settings.effort),
                    quiet)
        << "WebPConfigLosslessPreset() failed";
    config.exact = 1;
  } else {
    config.quality = input.codec_settings.quality;
    config.alpha_quality = input.codec_settings.quality;
    config.method = input.codec_settings.effort;
    config.use_sharp_yuv = 1;
  }
  config.thread_level = 0;

  if (original_image.size() == 1) {
    // Assume WebPEncode() below does not modify the pixels.
    ASSIGN_OR_RETURN(WebPPicture picture,
                     ArgbBufferToWebPPicture(const_cast<WP2::ArgbBuffer&>(
                                                 original_image.front().pixels),
                                             quiet));
    std::unique_ptr<WebPPicture, decltype(&R_WebPPictureFree)> picture_releaser(
        &picture, R_WebPPictureFree);
    picture.custom_ptr = &data;
    picture.writer = WriterFunction;
    CHECK_OR_RETURN(R_WebPEncode(&config, &picture), quiet)
        << "WebPEncode() failed: " << picture.error_code;
  } else {
    CHECK_OR_RETURN(false, quiet) << "libwebp-rs does not support animations";
  }
  return data;
}

StatusOr<std::pair<Image, double>> DecodeWebpRs(const TaskInput& input,
                                                const WP2::Data& encoded_image,
                                                bool quiet) {
  WebPDecoderConfig config;
  CHECK_OR_RETURN(R_WebPInitDecoderConfig(&config), quiet);
  config.output.colorspace = MODE_BGRA;
  config.options.use_threads = 0;
  const int status =
      R_WebPDecode(encoded_image.bytes, encoded_image.size, &config);
  CHECK_OR_RETURN(status == VP8_STATUS_OK, quiet)
      << "WebPDecode() failed: " << status;
  std::unique_ptr<WebPDecBuffer, decltype(&R_WebPFreeDecBuffer)>
      output_releaser(&config.output, R_WebPFreeDecBuffer);

  const Timer timer;
  WP2::ArgbBuffer buffer(WP2_BGRA_32);
  CHECK_OR_RETURN(
      buffer.Import(WP2_BGRA_32, config.output.width, config.output.height,
                    config.output.u.RGBA.rgba,
                    static_cast<uint32_t>(config.output.u.RGBA.stride)) ==
          WP2_STATUS_OK,
      quiet);
  const double color_conversion_duration = timer.seconds();

  Image image;
  image.reserve(1);
  image.emplace_back(std::move(buffer), /*duration_ms=*/0);
  return std::pair<Image, double>(std::move(image), color_conversion_duration);
}

#else
StatusOr<WP2::Data> EncodeWebpRs(const TaskInput&, const Image&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_WEBPRS";
}
StatusOr<std::pair<Image, double>> DecodeWebpRs(const TaskInput&,
                                                const WP2::Data&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_WEBPRS";
}
#endif  // HAS_WEBPRS

}  // namespace

CodecMetadata GetWebpRsMetadata() {
  return CodecMetadata{
      "webprs",
      WebpRsPrettyName,
      WebpRsVersion,
      " -DCCGEN_ENABLE_AVIF=OFF -DCCGEN_ENABLE_JPEG=OFF"
      " -DCCGEN_ENABLE_WEBPRS=ON",
      GetCodecMetadata(Codec::kWebp).efforts,
      GetCodecMetadata(Codec::kWebp).lossy_qualities,
      /*extension=*/"libwebp-rs.webp",
      /*mime_type=*/"image/webp",
      /*is_supported_by_browsers=*/true,
      /*supports_16bit=*/false,
      WebPPictureFormat(),
      WebPPictureFormat(),
      EncodeWebpRs,
      DecodeWebpRs,
  };
}

//------------------------------------------------------------------------------
// libwebp as the encoder and libwebp-rs as the decoder.

namespace {
std::string WebpEncWebpRsDecPrettyName(bool lossless, Subsampling subsampling,
                                       int effort) {
  return GetCodecMetadata(Codec::kWebp)
             .pretty_name(lossless, subsampling, effort) +
         " (libwebp-rs dec)";
}

std::string WebpEncWebpRsDecVersion() {
#if defined(HAS_WEBPRS)
  const std::string libwebp_enc_dec_version =
      GetCodecMetadata(Codec::kWebp).version();
  const std::string libwebp_enc_version =
      libwebp_enc_dec_version.substr(0, libwebp_enc_dec_version.find('/'));
  return "libwebp " + libwebp_enc_version + "/libwebp-rs " +
         VersionToString(R_WebPGetDecoderVersion());
#else
  return "n/a";
#endif
}
}  // namespace

CodecMetadata GetWebpEncWebpRsDecMetadata() {
  return CodecMetadata{
      "webpencwebprsdec",
      WebpEncWebpRsDecPrettyName,
      WebpEncWebpRsDecVersion,
      " -DCCGEN_ENABLE_AVIF=OFF -DCCGEN_ENABLE_JPEG=OFF"
      " -DCCGEN_ENABLE_WEBP=ON -DCCGEN_ENABLE_WEBPRS=ON",
      GetCodecMetadata(Codec::kWebp).efforts,
      GetCodecMetadata(Codec::kWebp).lossy_qualities,
      /*extension=*/"libwebpc.webp",
      /*mime_type=*/"image/webp",
      GetCodecMetadata(Codec::kWebp).is_supported_by_browsers,
      GetCodecMetadata(Codec::kWebp).supports_16bit,
      GetCodecMetadata(Codec::kWebp).opaque_format,
      GetCodecMetadata(Codec::kWebp).transparent_format,
      GetCodecMetadata(Codec::kWebp).encode,
      DecodeWebpRs,
  };
}

}  // namespace codec_compare_gen
