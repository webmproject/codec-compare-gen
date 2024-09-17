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

#include "src/frame.h"

#include <cstdint>

#if defined(HAS_WEBP2)
#include <fstream>
#include <iostream>
#include <utility>

#include "src/base.h"
#include "src/codec_webp.h"
#include "src/distortion.h"
#include "src/task.h"
#include "third_party/libwebp2/imageio/anim_image_dec.h"
#include "third_party/libwebp2/imageio/image_enc.h"
#include "third_party/libwebp2/src/wp2/base.h"
#endif  // HAS_WEBP2

namespace codec_compare_gen {

uint32_t GetDurationMs(const Image& image) {
  uint32_t duration_ms = 0;
  for (const Frame& frame : image) {
    duration_ms += frame.duration_ms;
  }
  return duration_ms;
}

#if defined(HAS_WEBP2)

StatusOr<Image> CloneAs(const Image& from, WP2SampleFormat format, bool quiet) {
  Image to;
  to.reserve(from.size());
  for (const Frame& frame : from) {
    to.emplace_back(WP2::ArgbBuffer(format), frame.duration_ms);
    CHECK_OR_RETURN(to.back().pixels.ConvertFrom(frame.pixels) == WP2_STATUS_OK,
                    quiet);
  }
  return to;
}

StatusOr<Image> MakeView(const Image& from, bool quiet) {
  Image to;
  to.reserve(from.size());
  for (const Frame& frame : from) {
    to.emplace_back(WP2::ArgbBuffer(frame.pixels.format()), frame.duration_ms);
    CHECK_OR_RETURN(to.back().pixels.SetView(frame.pixels) == WP2_STATUS_OK,
                    quiet);
  }
  return to;
}

StatusOr<Image> ReadStillImageOrAnimation(const char* file_path,
                                          WP2SampleFormat format, bool quiet) {
  // Reuse libwebp2's wrapper for simplicity.
  Image image;
  {
    WP2::ArgbBuffer buffer(WP2_ARGB_32);
    WP2::ImageReader reader(file_path, &buffer);
    bool is_last;
    do {
      uint32_t duration_ms;
      const WP2Status status = reader.ReadFrame(&is_last, &duration_ms);
      CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
          << "Got " << WP2GetStatusMessage(status) << " when reading frame "
          << image.size() << " of " << file_path;

      if (duration_ms == 0 && !is_last) {
        std::cout << "Warning: 0-second frame " << image.size() << " of "
                  << file_path << " was ignored" << std::endl;
        continue;
      }

      WP2::ArgbBuffer pixels(format);
      // All metadata is discarded during the conversion.
      CHECK_OR_RETURN(pixels.ConvertFrom(buffer) == WP2_STATUS_OK, quiet);

      if (!image.empty()) {
        ASSIGN_OR_RETURN(const bool pixel_equality,
                         PixelEquality(image.back().pixels, pixels, quiet));
        if (pixel_equality) {
          // Merge duplicate frames. Duplicate frames are fairly common in GIFs
          // found in the wild so no need to log them.
          image.back().duration_ms += duration_ms;
          continue;
        }
      }

      image.emplace_back(std::move(pixels), duration_ms);
    } while (!is_last);
  }
  CHECK_OR_RETURN(!image.empty(), quiet);
  return image;
}

Status WriteStillImageOrAnimation(const Image& image, const char* file_path,
                                  bool quiet) {
  if (image.size() == 1) {
    const WP2::ArgbBuffer& pixels = image.front().pixels;
    WP2Status status = WP2::SaveImage(pixels, file_path, /*overwrite=*/true);
    if (status == WP2_STATUS_UNSUPPORTED_FEATURE &&
        pixels.format() != WP2_Argb_32 && pixels.format() != WP2_ARGB_32) {
      // Try again with a format that is expected to be implemented.
      WP2::ArgbBuffer pixels4(
          WP2IsPremultiplied(pixels.format()) ? WP2_Argb_32 : WP2_ARGB_32);
      CHECK_OR_RETURN(pixels4.ConvertFrom(pixels) == WP2_STATUS_OK, quiet);
      status = WP2::SaveImage(pixels4, file_path, /*overwrite=*/true);
    }

    CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
        << "WP2::SaveImage(" << file_path
        << ") failed: " << WP2GetStatusMessage(status);
  } else {
    // Only WebP supports lossless animation encoding in this framework so far.
    // Keep whatever extension (.png) for the simplicity of the whole pipeline.
    const char* encoded_path = file_path;
    const TaskInput input = {
        {Codec::kWebp, Subsampling::k444, /*effort=*/9, kQualityLossless},
        /*image_path=*/file_path,  // For better logs.
        encoded_path};
    ASSIGN_OR_RETURN(const Image bgra,
                     image.front().pixels.format() == WP2_BGRA_32
                         ? MakeView(image, quiet)
                         : CloneAs(image, WP2_BGRA_32, quiet));
    ASSIGN_OR_RETURN(const WP2::Data encoded_image,
                     EncodeWebp(input, bgra, quiet));
    std::ofstream(file_path, std::ios::binary)
        .write(reinterpret_cast<char*>(encoded_image.bytes),
               encoded_image.size);
  }
  return Status::kOk;
}

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
