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

#include "src/codec_jpegsimple.h"

#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/codec_jpegturbo.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

#if defined(HAS_JPEGSIMPLE)
#include "third_party/sjpeg/src/sjpeg.h"
#endif

namespace codec_compare_gen {

std::string JpegsimpleVersion() {
#if defined(HAS_JPEGSIMPLE)
  return std::to_string(SjpegVersion() / 1000000) + "." +
         std::to_string(SjpegVersion() % 10000 / 100) + "." +
         std::to_string(SjpegVersion() % 100) + "_" + JpegturboVersion();
#else
  return "n/a";
#endif
}

std::vector<int> JpegsimpleLossyQualities() {
  std::vector<int> qualities(101);
  std::iota(qualities.begin(), qualities.end(), 0);
  return qualities;
}

#if defined(HAS_WEBP2)

#if defined(HAS_JPEGSIMPLE) && defined(HAS_JPEGTURBO)

StatusOr<WP2::Data> EncodeJpegsimple(const TaskInput& input,
                                     const WP2::ArgbBuffer& original_image,
                                     bool quiet) {
  CHECK_OR_RETURN(
      input.codec_settings.effort >= 0 && input.codec_settings.effort <= 8,
      quiet)
      << "sjpeg method " << input.codec_settings.effort
      << " must be between 0 and 8";
  CHECK_OR_RETURN(original_image.format() == WP2_RGB_24, quiet);

  const uint32_t stride =
      original_image.width() * WP2FormatBpp(original_image.format());
  uint8_t* buffer;
  const size_t size = SjpegEncode(
      original_image.GetRow8(0), static_cast<int>(original_image.width()),
      static_cast<int>(original_image.height()), static_cast<int>(stride),
      &buffer, input.codec_settings.quality, input.codec_settings.effort,
      SJPEG_YUV_444);
  CHECK_OR_RETURN(size != 0, quiet);

  // Copy the data to avoid a delete[]/free() mismatch.
  WP2::Data data;
  CHECK_OR_RETURN(data.CopyFrom(buffer, size) == WP2_STATUS_OK, quiet);
  SjpegFreeBuffer(buffer);
  return data;
}

StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeJpegsimple(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  return DecodeJpegturbo(input, encoded_image, quiet);
}

#else
StatusOr<WP2::Data> EncodeJpegsimple(const TaskInput&, const WP2::ArgbBuffer&,
                                     bool quiet) {
  CHECK_OR_RETURN(false, quiet)
      << "Encoding images requires HAS_JPEGSIMPLE and HAS_JPEGTURBO";
}
StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeJpegsimple(const TaskInput&,
                                                              const WP2::Data&,
                                                              bool quiet) {
  CHECK_OR_RETURN(false, quiet)
      << "Decoding images requires HAS_JPEGSIMPLE and HAS_JPEGTURBO";
}
#endif  // HAS_JPEGSIMPLE && HAS_JPEGTURBO

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
