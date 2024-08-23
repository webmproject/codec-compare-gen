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

#include "src/codec_webp.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/frame.h"
#include "src/task.h"
#include "src/timer.h"
#include "third_party/libwebp/src/webp/mux_types.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

#if defined(HAS_WEBP)
#include "third_party/libwebp/src/webp/decode.h"
#include "third_party/libwebp/src/webp/demux.h"
#include "third_party/libwebp/src/webp/encode.h"
#include "third_party/libwebp/src/webp/mux.h"
#endif

namespace codec_compare_gen {

namespace {
std::string VersionToString(int version) {
  return std::to_string((version >> 16) & 0xff) + "." +
         std::to_string((version >> 8) & 0xff) + "." +
         std::to_string(version & 0xff);
}
}  // namespace

std::string WebpVersion() {
#if defined(HAS_WEBP)
  if (WebPGetEncoderVersion() == WebPGetDecoderVersion()) {
    return VersionToString(WebPGetEncoderVersion());
  }
  return VersionToString(WebPGetEncoderVersion()) + "/" +
         VersionToString(WebPGetDecoderVersion());
#else
  return "n/a";
#endif
}

std::vector<int> WebpLossyQualities() {
  std::vector<int> qualities(101);
  std::iota(qualities.begin(), qualities.end(), 0);
  return qualities;  // [0:100]
}

#if defined(HAS_WEBP2)

WP2SampleFormat WebPPictureFormat() {
  const uint32_t is_little_endian = 1;
  return reinterpret_cast<const uint8_t*>(&is_little_endian)[0] ? WP2_BGRA_32
                                                                : WP2_ARGB_32;
}

#if defined(HAS_WEBP)

namespace {

// Returns a WebPPicture that points to the given ArgbBuffer.
StatusOr<WebPPicture> ArgbBufferToWebPPicture(WP2::ArgbBuffer& buffer,
                                              bool quiet) {
  WebPPicture picture = {};
  CHECK_OR_RETURN(WebPPictureInit(&picture), quiet);
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

}  // namespace

StatusOr<WP2::Data> EncodeWebp(const TaskInput& input,
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
  CHECK_OR_RETURN(WebPConfigInit(&config), quiet) << "WebPConfigInit() failed";
  if (lossless) {
    CHECK_OR_RETURN(WebPConfigLosslessPreset(
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
    std::unique_ptr<WebPPicture, decltype(&WebPPictureFree)> picture_releaser(
        &picture, WebPPictureFree);
    picture.custom_ptr = &data;
    picture.writer = WriterFunction;
    CHECK_OR_RETURN(WebPEncode(&config, &picture), quiet);
  } else {
    WebPAnimEncoderOptions enc_options;
    CHECK_OR_RETURN(WebPAnimEncoderOptionsInit(&enc_options), quiet);
    enc_options.minimize_size = config.method >= 5;  // arbitrary
    enc_options.allow_mixed = !lossless;
    std::unique_ptr<WebPAnimEncoder, decltype(&WebPAnimEncoderDelete)> enc(
        WebPAnimEncoderNew(width, height, &enc_options), WebPAnimEncoderDelete);
    CHECK_OR_RETURN(enc != nullptr, quiet);

    int timestamp_ms = 0;
    for (const Frame& frame : original_image) {
      // Assume WebPAnimEncoderAdd() below does not modify the pixels.
      ASSIGN_OR_RETURN(WebPPicture picture,
                       ArgbBufferToWebPPicture(
                           const_cast<WP2::ArgbBuffer&>(frame.pixels), quiet));
      std::unique_ptr<WebPPicture, decltype(&WebPPictureFree)> picture_releaser(
          &picture, WebPPictureFree);
      CHECK_OR_RETURN(
          WebPAnimEncoderAdd(enc.get(), &picture, timestamp_ms, &config),
          quiet);
      timestamp_ms += static_cast<int>(frame.duration_ms);
    }
    CHECK_OR_RETURN(
        WebPAnimEncoderAdd(enc.get(), nullptr, timestamp_ms, &config), quiet);
    WebPData webp_data;
    WebPDataInit(&webp_data);
    CHECK_OR_RETURN(WebPAnimEncoderAssemble(enc.get(), &webp_data), quiet);
    data.bytes = const_cast<uint8_t*>(webp_data.bytes);
    data.size = webp_data.size;
  }
  return data;
}

StatusOr<std::pair<Image, double>> DecodeWebp(const TaskInput& input,
                                              const WP2::Data& encoded_image,
                                              bool quiet) {
  WebPAnimDecoderOptions dec_options;
  CHECK_OR_RETURN(WebPAnimDecoderOptionsInit(&dec_options), quiet);
  dec_options.color_mode = MODE_BGRA;
  dec_options.use_threads = 0;
  const WebPData webp_data = {encoded_image.bytes, encoded_image.size};
  std::unique_ptr<WebPAnimDecoder, decltype(&WebPAnimDecoderDelete)> dec(
      WebPAnimDecoderNew(&webp_data, &dec_options), WebPAnimDecoderDelete);
  CHECK_OR_RETURN(dec != nullptr, quiet);

  WebPAnimInfo anim_info;
  CHECK_OR_RETURN(WebPAnimDecoderGetInfo(dec.get(), &anim_info), quiet);

  Image image;
  image.reserve(anim_info.frame_count);
  int previous_timestamp = 0;
  while (WebPAnimDecoderHasMoreFrames(dec.get())) {
    uint8_t* buf;
    int timestamp;
    CHECK_OR_RETURN(WebPAnimDecoderGetNext(dec.get(), &buf, &timestamp), quiet);

    // This does not depend on endianness so no need for WebPPictureFormat().
    WP2::ArgbBuffer buffer(WP2_BGRA_32);
    CHECK_OR_RETURN(
        buffer.Import(buffer.format(), anim_info.canvas_width,
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
StatusOr<WP2::Data> EncodeWebp(const TaskInput&, const Image&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_WEBP";
}
StatusOr<std::pair<Image, double>> DecodeWebp(const TaskInput&,
                                              const WP2::Data&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_WEBP";
}
#endif  // HAS_WEBP

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
