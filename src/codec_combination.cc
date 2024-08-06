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

#include "src/codec_combination.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/codec_jpegxl.h"
#include "src/codec_webp.h"
#include "src/codec_webp2.h"
#include "src/frame.h"
#include "src/task.h"
#include "src/timer.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

namespace codec_compare_gen {

std::string CodecCombinationVersion() {
  return WebpVersion() + "_" + Webp2Version() + "_" + JpegXLVersion();
}

std::vector<int> CodecCombinationLossyQualities() {
  std::vector<int> qualities(91);
  std::iota(qualities.begin(), qualities.end(), 5);
  return qualities;  // [5:95] so that every quality works with each codec.
}

#if defined(HAS_WEBP2)

namespace {

bool HasTransparency(const Image& image) {
  for (const Frame& frame : image) {
    if (frame.pixels.HasTransparency()) return true;
  }
  return false;
}

struct CodecEffort {
  Codec codec;
  int effort;
};

}  // namespace

StatusOr<WP2::Data> EncodeCodecCombination(const TaskInput& input,
                                           const Image& original_image,
                                           bool quiet) {
  constexpr CodecEffort kNone = {Codec::kCombination, -1};
  constexpr int kMaxNumCodecs = 3;
  constexpr int kMaxEffort = 9;
  // Arbitrary mapping from input effort to codec combination.
  constexpr CodecEffort kCombinations[kMaxEffort + 1][kMaxNumCodecs] = {
      /*0=*/{{Codec::kJpegXl, /*effort=*/1}, kNone, kNone},
      /*1=*/{{Codec::kWebp, 1}, kNone, kNone},
      /*2=*/{{Codec::kWebp, 2}, kNone, kNone},
      /*3=*/{{Codec::kWebp, 3}, kNone, kNone},
      /*4=*/{{Codec::kWebp, 4}, kNone, kNone},
      /*5=*/{{Codec::kWebp, 6}, kNone, kNone},
      /*6=*/{{Codec::kWebp, 6}, {Codec::kJpegXl, 2}, kNone},
      /*7=*/{{Codec::kWebp, 6}, {Codec::kWebp2, 3}, {Codec::kJpegXl, 2}},
      /*8=*/{{Codec::kWebp, 6}, {Codec::kWebp2, 3}, {Codec::kJpegXl, 9}},
      /*9=*/{{Codec::kWebp, 6}, {Codec::kWebp2, 5}, {Codec::kJpegXl, 9}},
  };
  CHECK_OR_RETURN(input.codec_settings.effort >= 0 &&
                      input.codec_settings.effort <= kMaxEffort,
                  quiet)
      << "Invalid effort " << input.codec_settings.effort;
  const CodecEffort* combination = kCombinations[input.codec_settings.effort];

  WP2::Data data;
  for (int i = 0; i < kMaxNumCodecs; ++i) {
    const TaskInput specialized_input = {
        {combination[i].codec, input.codec_settings.chroma_subsampling,
         combination[i].effort, input.codec_settings.quality},
        input.image_path};
    if (specialized_input.codec_settings.effort == kNone.effort) break;

    using std::swap;
    if (specialized_input.codec_settings.codec == Codec::kWebp) {
      ASSIGN_OR_RETURN(const Image image,
                       CloneAs(original_image, WebPPictureFormat(), quiet));
      ASSIGN_OR_RETURN(WP2::Data candidate,
                       EncodeWebp(specialized_input, image, quiet));
      if (data.IsEmpty() || candidate.size < data.size) swap(data, candidate);
    } else if (specialized_input.codec_settings.codec == Codec::kWebp2) {
      ASSIGN_OR_RETURN(WP2::Data candidate,
                       EncodeWebp2(specialized_input, original_image, quiet));
      if (data.IsEmpty() || candidate.size < data.size) swap(data, candidate);
    } else {
      assert(specialized_input.codec_settings.codec == Codec::kJpegXl);
      const WP2SampleFormat jxl_format =
          HasTransparency(original_image) ? WP2_RGBA_32 : WP2_RGB_24;
      ASSIGN_OR_RETURN(const Image image,
                       CloneAs(original_image, jxl_format, quiet));
      ASSIGN_OR_RETURN(WP2::Data candidate,
                       EncodeJxl(specialized_input, image, quiet));
      if (data.IsEmpty() || candidate.size < data.size) swap(data, candidate);
    }
  }
  return data;
}

StatusOr<std::pair<Image, double>> DecodeCodecCombination(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  if (encoded_image.size >= 12 &&
      std::equal(encoded_image.bytes, encoded_image.bytes + 4, "RIFF") &&
      std::equal(encoded_image.bytes + 8, encoded_image.bytes + 12, "WEBP")) {
    ASSIGN_OR_RETURN(auto webp, DecodeWebp(input, encoded_image, quiet));
    const Timer color_conversion_duration;
    ASSIGN_OR_RETURN(Image clone, CloneAs(webp.first, WP2_ARGB_32, quiet));
    return std::pair<Image, double>(
        std::move(clone), webp.second + color_conversion_duration.seconds());
  }

  if (encoded_image.size >= 3 && encoded_image.bytes[0] == 0xf4 &&
      encoded_image.bytes[1] == 0xff && encoded_image.bytes[2] == 0x6f) {
    return DecodeWebp2(input, encoded_image, quiet);
  }

  ASSIGN_OR_RETURN(auto jxl, DecodeJxl(input, encoded_image, quiet));
  const Timer color_conversion_duration;
  ASSIGN_OR_RETURN(Image clone, CloneAs(jxl.first, WP2_ARGB_32, quiet));
  return std::pair<Image, double>(
      std::move(clone), jxl.second + color_conversion_duration.seconds());
}

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
