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
#include "webp_rs/demux_prefix.h"
#include "webp_rs/encode_prefix.h"
#include "webp_rs/mux_prefix.h"
#include "webp_rs/mux_types_prefix.h"
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

  const int width = static_cast<int>(original_image.front().pixels.width());
  const int height = static_cast<int>(original_image.front().pixels.height());

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
    WebPAnimEncoderOptions enc_options;
    CHECK_OR_RETURN(R_WebPAnimEncoderOptionsInit(&enc_options), quiet);
    enc_options.minimize_size = config.method >= 5;  // arbitrary
    enc_options.allow_mixed = !lossless;
    std::unique_ptr<WebPAnimEncoder, decltype(&R_WebPAnimEncoderDelete)> enc(
        R_WebPAnimEncoderNew(width, height, &enc_options),
        R_WebPAnimEncoderDelete);
    CHECK_OR_RETURN(enc != nullptr, quiet) << "WebPAnimEncoderNew() failed";

    int timestamp_ms = 0;
    for (const Frame& frame : original_image) {
      // Assume WebPAnimEncoderAdd() below does not modify the pixels.
      ASSIGN_OR_RETURN(WebPPicture picture,
                       ArgbBufferToWebPPicture(
                           const_cast<WP2::ArgbBuffer&>(frame.pixels), quiet));
      std::unique_ptr<WebPPicture, decltype(&R_WebPPictureFree)>
          picture_releaser(&picture, R_WebPPictureFree);
      CHECK_OR_RETURN(
          R_WebPAnimEncoderAdd(enc.get(), &picture, timestamp_ms, &config),
          quiet)
          << "WebPAnimEncoderAdd() failed: "
          << R_WebPAnimEncoderGetError(enc.get());
      timestamp_ms += static_cast<int>(frame.duration_ms);
    }
    CHECK_OR_RETURN(
        R_WebPAnimEncoderAdd(enc.get(), nullptr, timestamp_ms, &config), quiet)
        << "WebPAnimEncoderAdd() failed: "
        << R_WebPAnimEncoderGetError(enc.get());
    WebPData webp_data;
    R_WebPDataInit(&webp_data);
    std::unique_ptr<WebPData, decltype(&R_WebPDataClear)> webp_data_releaser(
        &webp_data, R_WebPDataClear);
    CHECK_OR_RETURN(R_WebPAnimEncoderAssemble(enc.get(), &webp_data), quiet)
        << "WebPAnimEncoderAssemble() failed: "
        << R_WebPAnimEncoderGetError(enc.get());
    // The bytes were allocated by R_WebPMalloc() so they must be released by
    // R_WebPDataClear(). They cannot be owned by WP2::Data, unlike in
    // EncodeWebp().
    CHECK_OR_RETURN(
        data.CopyFrom(webp_data.bytes, webp_data.size) == WP2_STATUS_OK, quiet);
  }
  return data;
}

StatusOr<std::pair<Image, double>> DecodeWebpRs(const TaskInput& input,
                                                const WP2::Data& encoded_image,
                                                bool quiet) {
  WebPAnimDecoderOptions dec_options;
  CHECK_OR_RETURN(R_WebPAnimDecoderOptionsInit(&dec_options), quiet);
  dec_options.color_mode = MODE_BGRA;
  dec_options.use_threads = 0;
  const WebPData webp_data = {encoded_image.bytes, encoded_image.size};
  std::unique_ptr<WebPAnimDecoder, decltype(&R_WebPAnimDecoderDelete)> dec(
      R_WebPAnimDecoderNew(&webp_data, &dec_options), R_WebPAnimDecoderDelete);
  CHECK_OR_RETURN(dec != nullptr, quiet) << "WebPAnimDecoderNew() failed";

  WebPAnimInfo anim_info;
  CHECK_OR_RETURN(R_WebPAnimDecoderGetInfo(dec.get(), &anim_info), quiet);

  Image image;
  image.reserve(anim_info.frame_count);
  int previous_timestamp = 0;
  double color_conversion_duration = 0;
  while (R_WebPAnimDecoderHasMoreFrames(dec.get())) {
    uint8_t* buf;
    int timestamp;
    CHECK_OR_RETURN(R_WebPAnimDecoderGetNext(dec.get(), &buf, &timestamp),
                    quiet)
        << "WebPAnimDecoderGetNext() failed";

    // This does not depend on endianness so no need for WebPPictureFormat().
    const Timer timer;
    WP2::ArgbBuffer buffer(WP2_BGRA_32);
    CHECK_OR_RETURN(
        buffer.Import(buffer.format(), anim_info.canvas_width,
                      anim_info.canvas_height, buf,
                      anim_info.canvas_width * WP2FormatBpp(buffer.format())) ==
            WP2_STATUS_OK,
        quiet);
    color_conversion_duration += timer.seconds();
    image.emplace_back(std::move(buffer),
                       static_cast<uint32_t>(timestamp - previous_timestamp));
    previous_timestamp = timestamp;
  }
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
