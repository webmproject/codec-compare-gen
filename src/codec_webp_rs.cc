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
#include "src/codec_webp.h"
#include "src/frame.h"
#include "src/task.h"
#include "src/timer.h"
#include "src/wp2/base.h"

#if defined(HAS_WEBPRS)
#include "bindings/webp/decode_rs.h"
#include "bindings/webp/demux_rs.h"
#include "bindings/webp/encode_rs.h"
#include "bindings/webp/mux_rs.h"
#include "bindings/webp/mux_types_rs.h"
#endif

// The image crate as the encoder and the image-webp crate as the decoder.

namespace codec_compare_gen {
namespace {

std::string VersionToString(int version) {
  return std::to_string((version >> 16) & 0xff) + "." +
         std::to_string((version >> 8) & 0xff) + "." +
         std::to_string(version & 0xff);
}

std::string WebpRsPrettyName(bool lossless, Subsampling subsampling,
                             int effort) {
  assert(lossless);
  assert(subsampling == Subsampling::k444 ||
         subsampling == Subsampling::kDefault);
  assert(effort == 0);
  return "WebP-rs";
}

std::string WebpRsVersion() {
#if defined(HAS_WEBPRS)
  return "image-rs" + VersionToString(ccgen_WebPGetEncoderVersion()) +
         "/image-webp-rs" + VersionToString(ccgen_WebPGetDecoderVersion());
#else
  return "n/a";
#endif
}

std::vector<int> WebpRsLossyQualities() {
  // The image crate does not support lossy WebP encoding as of May 2026.
  return {};
}

#if defined(HAS_WEBPRS)

// Returns a WebPPicture that points to the given ArgbBuffer.
StatusOr<ccgen_WebPPicture> ArgbBufferToWebPPicture(WP2::ArgbBuffer& buffer,
                                                    bool quiet) {
  ccgen_WebPPicture picture = {};
  CHECK_OR_RETURN(ccgen_WebPPictureInit(&picture), quiet);
  picture.use_argb = 1;
  picture.width = static_cast<int>(buffer.width());
  picture.height = static_cast<int>(buffer.height());
  // Avoid WebPPictureAlloc() and a copy.
  CHECK_OR_RETURN(
      buffer.format() == GetCodecMetadata(Codec::kWebp).transparent_format,
      quiet);
  picture.argb =
      reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(buffer.GetRow8(0)));
  picture.argb_stride =
      static_cast<int>(buffer.stride()) / WP2FormatBpp(buffer.format());
  return picture;
}

// WebPWriterFunction implementation.
int WriterFunction(const uint8_t* data, size_t data_size,
                   const ccgen_WebPPicture* picture) {
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
  ccgen_WebPConfig config;
  CHECK_OR_RETURN(ccgen_WebPConfigInit(&config), quiet)
      << "WebPConfigInit() failed";
  if (lossless) {
    CHECK_OR_RETURN(ccgen_WebPConfigLosslessPreset(
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
    ASSIGN_OR_RETURN(ccgen_WebPPicture picture,
                     ArgbBufferToWebPPicture(const_cast<WP2::ArgbBuffer&>(
                                                 original_image.front().pixels),
                                             quiet));
    std::unique_ptr<ccgen_WebPPicture, decltype(&ccgen_WebPPictureFree)>
        picture_releaser(&picture, ccgen_WebPPictureFree);
    picture.custom_ptr = &data;
    picture.writer = WriterFunction;
    CHECK_OR_RETURN(ccgen_WebPEncode(&config, &picture), quiet)
        << "ccgen_WebPEncode() failed: " << picture.error_code;
  } else {
    ccgen_WebPAnimEncoderOptions enc_options;
    CHECK_OR_RETURN(ccgen_WebPAnimEncoderOptionsInit(&enc_options), quiet);
    enc_options.minimize_size = config.method >= 5;  // arbitrary
    enc_options.allow_mixed = !lossless;
    std::unique_ptr<ccgen_WebPAnimEncoder,
                    decltype(&ccgen_WebPAnimEncoderDelete)>
        enc(ccgen_WebPAnimEncoderNew(width, height, &enc_options),
            ccgen_WebPAnimEncoderDelete);
    CHECK_OR_RETURN(enc != nullptr, quiet);

    int timestamp_ms = 0;
    for (const Frame& frame : original_image) {
      // Assume WebPAnimEncoderAdd() below does not modify the pixels.
      ASSIGN_OR_RETURN(ccgen_WebPPicture picture,
                       ArgbBufferToWebPPicture(
                           const_cast<WP2::ArgbBuffer&>(frame.pixels), quiet));
      std::unique_ptr<ccgen_WebPPicture, decltype(&ccgen_WebPPictureFree)>
          picture_releaser(&picture, ccgen_WebPPictureFree);
      CHECK_OR_RETURN(
          ccgen_WebPAnimEncoderAdd(enc.get(), &picture, timestamp_ms, &config),
          quiet)
          << "ccgen_WebPAnimEncoderAdd() failed: " << picture.error_code;
      timestamp_ms += static_cast<int>(frame.duration_ms);
    }
    CHECK_OR_RETURN(
        ccgen_WebPAnimEncoderAdd(enc.get(), nullptr, timestamp_ms, &config),
        quiet);
    ccgen_WebPData webp_data;
    ccgen_WebPDataInit(&webp_data);
    CHECK_OR_RETURN(ccgen_WebPAnimEncoderAssemble(enc.get(), &webp_data),
                    quiet);
    data.bytes = const_cast<uint8_t*>(webp_data.bytes);
    data.size = webp_data.size;
  }
  return data;
}

StatusOr<std::pair<Image, double>> DecodeWebpRs(const TaskInput& input,
                                                const WP2::Data& encoded_image,
                                                bool quiet) {
  ccgen_WebPAnimDecoderOptions dec_options;
  CHECK_OR_RETURN(ccgen_WebPAnimDecoderOptionsInit(&dec_options), quiet);

  // Encoding with the Rust image-webp crate uses a packed ARGB buffer, meaning
  // non-packed BGRA in little-endian (just like the libwebp API). However,
  // decoding with the Rust image-webp crate only supports a non-packed RGB(A)
  // output buffer (the libwebp API supports more patterns).
  dec_options.color_mode = MODE_RGBA;  // Non-packed WP2_RGBA_32 imported below.
  // Keep the buffer below as non-packed WP2_BGRA_32 for enc/dec consistency.

  dec_options.use_threads = 0;
  const ccgen_WebPData webp_data = {encoded_image.bytes, encoded_image.size};
  std::unique_ptr<ccgen_WebPAnimDecoder, decltype(&ccgen_WebPAnimDecoderDelete)>
      dec(ccgen_WebPAnimDecoderNew(&webp_data, &dec_options),
          ccgen_WebPAnimDecoderDelete);
  CHECK_OR_RETURN(dec != nullptr, quiet);

  ccgen_WebPAnimInfo anim_info;
  CHECK_OR_RETURN(ccgen_WebPAnimDecoderGetInfo(dec.get(), &anim_info), quiet);

  Image image;
  image.reserve(anim_info.frame_count);
  int previous_timestamp = 0;
  while (ccgen_WebPAnimDecoderHasMoreFrames(dec.get())) {
    uint8_t* buf;
    int timestamp;
    CHECK_OR_RETURN(ccgen_WebPAnimDecoderGetNext(dec.get(), &buf, &timestamp),
                    quiet);

    WP2::ArgbBuffer buffer(WP2_BGRA_32);
    CHECK_OR_RETURN(
        buffer.Import(WP2_RGBA_32, anim_info.canvas_width,
                      anim_info.canvas_height, buf,
                      anim_info.canvas_width * WP2FormatBpp(buffer.format())) ==
            WP2_STATUS_OK,
        quiet);
    image.emplace_back(std::move(buffer),
                       static_cast<uint32_t>(timestamp - previous_timestamp));
    previous_timestamp = timestamp;
  }
  return std::pair<Image, double>(std::move(image), 0);
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
      WebpRsLossyQualities,
      "image-rs.webp",
      /*is_supported_by_browsers=*/true,
      GetCodecMetadata(Codec::kWebp).supports_16bit,
      GetCodecMetadata(Codec::kWebp).transparent_format,
      GetCodecMetadata(Codec::kWebp).opaque_format,
      EncodeWebpRs,
      DecodeWebpRs,
  };
}

//------------------------------------------------------------------------------
// libwebp as the encoder and the image-webp crate as the decoder.

namespace {
std::string WebpEncWebpRsDecPrettyName(bool lossless, Subsampling subsampling,
                                       int effort) {
  return GetWebpMetadata().pretty_name(lossless, subsampling, effort) +
         " (rs dec)";
}

std::string WebpEncWebpRsDecVersion() {
#if defined(HAS_WEBPRS)
  const std::string libwebp_enc_dec_version = GetWebpMetadata().version();
  const std::string libwebp_enc_version =
      libwebp_enc_dec_version.substr(0, libwebp_enc_dec_version.find('/'));
  return "libwebp" + libwebp_enc_version + "/image-webp-rs" +
         VersionToString(ccgen_WebPGetDecoderVersion());
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
      GetWebpMetadata().lossy_qualities,
      "libwebp.webp",
      GetWebpMetadata().is_supported_by_browsers,
      GetCodecMetadata(Codec::kWebp).supports_16bit,
      GetCodecMetadata(Codec::kWebp).transparent_format,
      GetCodecMetadata(Codec::kWebp).opaque_format,
      GetWebpMetadata().encode,
      DecodeWebpRs,
  };
}

}  // namespace codec_compare_gen
