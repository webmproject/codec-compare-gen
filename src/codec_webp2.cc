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

#include "src/codec_webp2.h"

#include <cassert>
#include <cstdint>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/frame.h"
#include "src/framework.h"
#include "src/serialization.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#include "third_party/libwebp2/src/wp2/decode.h"
#include "third_party/libwebp2/src/wp2/encode.h"
#include "third_party/libwebp2/src/wp2/format_constants.h"
#endif  // HAS_WEBP2

namespace codec_compare_gen {

namespace {
std::string VersionToString(int version) {
  return std::to_string((version >> 16) & 0xff) + "." +
         std::to_string((version >> 8) & 0xff) + "." +
         std::to_string(version & 0xff);
}
}  // namespace

std::string Webp2Version() {
#if defined(HAS_WEBP2)
  return VersionToString(WP2GetVersion());
#else
  return "n/a";
#endif  // HAS_WEBP2
}

std::vector<int> Webp2LossyQualities() {
  std::vector<int> qualities(96);
  std::iota(qualities.begin(), qualities.end(), 0);
  return qualities;  // [0:95] because [96:100] is near-lossless and lossless.
}

#if defined(HAS_WEBP2)

StatusOr<WP2::Data> EncodeWebp2(const TaskInput& input,
                                const Image& original_image, bool quiet) {
  WP2::Data data;
  WP2::DataWriter writer(&data);
  WP2::EncoderConfig config;
  if (input.codec_settings.quality == kQualityLossless) {
    config.quality = 100.0f;
    config.alpha_quality = 100.0f;
    config.keep_unmultiplied = true;
    config.tile_shape = WP2::TILE_SHAPE_WIDE;
  } else {
    config.quality = input.codec_settings.quality;
    config.alpha_quality = input.codec_settings.quality;
  }
  if (input.codec_settings.chroma_subsampling == Subsampling::kDefault) {
    config.uv_mode = WP2::EncoderConfig::UVModeAuto;
  } else if (input.codec_settings.chroma_subsampling == Subsampling::k420) {
    config.uv_mode = WP2::EncoderConfig::UVMode420;
  } else {
    CHECK_OR_RETURN(
        input.codec_settings.chroma_subsampling == Subsampling::k444, quiet)
        << "WebP2 does not support chroma subsampling "
        << SubsamplingToString(input.codec_settings.chroma_subsampling);
    config.uv_mode = WP2::EncoderConfig::UVMode444;
  }
  config.effort = input.codec_settings.effort;
  config.thread_level = 0;
  if (original_image.size() == 1) {
    const WP2Status status =
        WP2::Encode(original_image.front().pixels, &writer, config);
    CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
        << "WP2::Encode() failed with \"" << WP2GetStatusMessage(status)
        << "\" when encoding " << input.image_path;
  } else {
    WP2::AnimationEncoder encoder;
    for (const Frame& frame : original_image) {
      const WP2Status status =
          encoder.AddFrame(frame.pixels, frame.duration_ms);
      CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
          << "WP2::AnimationEncoder::AddFrame() failed with \""
          << WP2GetStatusMessage(status) << "\" when encoding "
          << input.image_path;
    }
    const WP2Status status = encoder.Encode(&writer, config);
    CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
        << "WP2::AnimationEncoder::Encode() failed with \""
        << WP2GetStatusMessage(status) << "\" when encoding "
        << input.image_path;
  }
  return data;
}

StatusOr<std::pair<Image, double>> DecodeWebp2(const TaskInput& input,
                                               const WP2::Data& encoded_image,
                                               bool quiet) {
  // TODO: Fix the following error when compiled with gcc --enable-default-pie:
  //         codec_webp2.cc.o:(.data.rel.ro.ArrayDecoderE):
  //         undefined reference to typeinfo for WP2::Decoder

  WP2::DecoderConfig config;
  config.thread_level = 0;
  WP2::ArrayDecoder decoder(encoded_image.bytes, encoded_image.size, config);
  Image image;

  uint32_t duration_ms;
  while (decoder.ReadFrame(&duration_ms)) {
    image.emplace_back(WP2::ArgbBuffer(WP2_ARGB_32), duration_ms);
    CHECK_OR_RETURN(
        image.back().pixels.ConvertFrom(decoder.GetPixels()) == WP2_STATUS_OK,
        quiet);
  }
  CHECK_OR_RETURN(decoder.GetStatus() == WP2_STATUS_OK, quiet)
      << decoder.GetStatus();
  return std::pair<Image, double>(std::move(image), 0);
}

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
