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
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/framework.h"
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
                                const WP2::ArgbBuffer& original_image,
                                bool quiet) {
  WP2::Data data;
  WP2::DataWriter writer(&data);
  WP2::EncoderConfig config;
  if (input.codec_settings.quality == kQualityLossless) {
    config.quality = 100.0f;
    config.alpha_quality = 100.0f;
    config.exact = true;
    config.tile_shape = WP2::TILE_SHAPE_WIDE;
  } else {
    config.quality = input.codec_settings.quality;
    config.alpha_quality = input.codec_settings.quality;
  }
  config.effort = input.codec_settings.effort;
  config.thread_level = 0;
  const WP2Status status = WP2::Encode(original_image, &writer, config);
  CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
      << "WP2::Encode() failed with \"" << WP2GetStatusMessage(status)
      << "\" when encoding " << input.image_path;
  return data;
}

StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeWebp2(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  // TODO(yguyon): Measure color conversion time.
  std::pair<WP2::ArgbBuffer, double> image_and_color_conversion_duration(
      WP2_ARGB_32, 0.);
  WP2::DecoderConfig config;
  if (input.codec_settings.quality == kQualityLossless) {
    config.exact = true;
  }
  config.thread_level = 0;
  const WP2Status status =
      WP2::Decode(encoded_image.bytes, encoded_image.size,
                  &image_and_color_conversion_duration.first, config);
  CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
      << "WP2::Decode() failed with \"" << WP2GetStatusMessage(status)
      << "\" when decoding WebP2 " << input.image_path;
  return image_and_color_conversion_duration;
}

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
