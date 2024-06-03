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
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/task.h"
#include "src/timer.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/imageio/file_format.h"
#include "third_party/libwebp2/imageio/image_dec.h"
#include "third_party/libwebp2/imageio/image_enc.h"
#include "third_party/libwebp2/imageio/imageio_util.h"
#include "third_party/libwebp2/src/wp2/base.h"
#endif

#if defined(WP2_HAVE_WEBP)
#include "third_party/libwebp/src/webp/decode.h"
#include "third_party/libwebp/src/webp/encode.h"
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
#if defined(WP2_HAVE_WEBP)
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

#if defined(WP2_HAVE_WEBP)

StatusOr<WP2::Data> EncodeWebp(const TaskInput& input,
                               const WP2::ArgbBuffer& original_image,
                               bool quiet) {
  // Reuse libwebp2's wrapper for simplicity.
  WP2::Data data;
  WP2::DataWriter writer(&data);
  WebPConfig config;
  CHECK_OR_RETURN(WebPConfigInit(&config), quiet) << "WebPConfigInit() failed";
  if (input.codec_settings.quality == kQualityLossless) {
    CHECK_OR_RETURN(WebPConfigLosslessPreset(
                        &config, /*level=*/input.codec_settings.effort),
                    quiet)
        << "WebPConfigLosslessPreset() failed";
    config.exact = 1;
  } else {
    config.quality = input.codec_settings.quality;
    config.alpha_quality = input.codec_settings.quality;
    config.method = input.codec_settings.effort;
  }
  config.thread_level = 0;
  const WP2Status status = WP2::CompressWebP(original_image, config, &writer);
  CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
      << "CompressWebP() failed with \"" << WP2GetStatusMessage(status)
      << "\" when encoding " << input.image_path;
  return data;
}

StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeWebp(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  // Reuse libwebp2's wrapper for simplicity.
  // TODO(yguyon): Measure color conversion time.
  std::pair<WP2::ArgbBuffer, double> image_and_color_conversion_duration(
      WP2_ARGB_32, 0.);
  const WP2Status status = WP2::ReadImage(
      encoded_image.bytes, encoded_image.size,
      &image_and_color_conversion_duration.first, WP2::FileFormat::AUTO,
      quiet ? WP2::LogLevel::QUIET : WP2::LogLevel::VERBOSE);
  CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
      << "ReadImage() failed with \"" << WP2GetStatusMessage(status)
      << "\" when decoding WebP " << input.image_path;
  return image_and_color_conversion_duration;
}

#else
StatusOr<WP2::Data> EncodeWebp(const TaskInput&, const WP2::ArgbBuffer&,
                               bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires WP2_HAVE_WEBP";
}
StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeWebp(const TaskInput&,
                                                        const WP2::Data&,
                                                        bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires WP2_HAVE_WEBP";
}
#endif  // WP2_HAVE_WEBP

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
