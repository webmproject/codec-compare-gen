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

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>
#include <utility>
#include <vector>

#include "imageio/anim_image_dec.h"
#include "imageio/image_enc.h"
#include "src/base.h"
#include "src/codec.h"
#include "src/distortion.h"
#include "src/task.h"
#include "src/wp2/base.h"

namespace codec_compare_gen {

uint32_t GetDurationMs(const Image& image) {
  uint32_t duration_ms = 0;
  for (const Frame& frame : image) {
    duration_ms += frame.duration_ms;
  }
  return duration_ms;
}

StatusOr<Image> CloneAs(const Image& from, WP2SampleFormat format, bool quiet) {
  Image to;
  to.reserve(from.size());
  for (const Frame& frame : from) {
    to.emplace_back(WP2::ArgbBuffer(format), frame.duration_ms);
    CHECK_OR_RETURN(to.back().pixels.ConvertFrom(frame.pixels) == WP2_STATUS_OK,
                    quiet);
    // Check that there was no bit depth loss.
    CHECK_OR_RETURN(WP2Formatbpc(to.back().pixels.format()) ==
                        WP2Formatbpc(frame.pixels.format()),
                    quiet);
  }
  return to;
}

StatusOr<Image> SpreadTo8bit(const Image& from, bool quiet) {
  Image to;
  to.reserve(from.size());
  for (const Frame& frame : from) {
    const WP2SampleFormat format = WP2FormatAtbpc(frame.pixels.format(), 8);
    CHECK_OR_RETURN(format != WP2_FORMAT_NUM, quiet);
    to.emplace_back(WP2::ArgbBuffer(format), frame.duration_ms);
    CHECK_OR_RETURN(to.back().pixels.Resize(
                        frame.pixels.width() *
                            (WP2FormatBpp(frame.pixels.format()) /
                             WP2FormatNumChannels(frame.pixels.format())),
                        frame.pixels.height()) == WP2_STATUS_OK,
                    quiet);
    const uint32_t num_samples_per_row =
        WP2FormatNumChannels(frame.pixels.format()) * frame.pixels.width();
    for (uint32_t y = 0; y < frame.pixels.height(); ++y) {
      const uint16_t* src = frame.pixels.GetRow16(y);
      uint8_t* dst = to.back().pixels.GetRow8(y);
      for (uint32_t i = 0; i < num_samples_per_row; ++i) {
        dst[i] = src[i] >> 8;
        dst[i + num_samples_per_row] = src[i] & 0xFF;
      }
    }
    // Note: It would be simpler to use to.back().pixels.Import(format,
    //       frame.pixels) and consider 16-bit samples as twice as many 8-bit
    //       samples per row, but the resulting image of alternating low and
    //       high significant parts of the 16-bit samples is terribly hard and
    //       slow to compress.
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
  std::error_code ec;
  const uintmax_t file_size = std::filesystem::file_size(file_path, ec);
  CHECK_OR_RETURN(!ec, quiet)
      << "Cannot get the file size of " << file_path << ": " << ec.message();
  std::vector<uint8_t> bytes(file_size);
  std::ifstream file(file_path, std::ios::binary);
  CHECK_OR_RETURN(file.good(), quiet) << "Cannot open file " << file_path;
  file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
  return ReadStillImageOrAnimation(file_path, bytes.data(), bytes.size(),
                                   format, quiet);
}

StatusOr<Image> ReadStillImageOrAnimation(const char* data_source,
                                          const uint8_t* data, size_t data_size,
                                          WP2SampleFormat format, bool quiet) {
  // Reuse libwebp2's wrapper for simplicity.
  Image image;
  {
    WP2::ArgbBuffer buffer(WP2_ARGB_32);
    WP2::ImageReader reader(data, data_size, &buffer);
    bool is_last;
    do {
      uint32_t duration_ms;
      WP2Status status = reader.ReadFrame(&is_last, &duration_ms);
      if (status == WP2_STATUS_INVALID_PARAMETER && image.empty()) {
        // Maybe it is a 16-bit file and the ImageReaderPNG refused to read it
        // into an 8-bit buffer. Try again with a 16-bit buffer.
        CHECK_OR_RETURN(buffer.SetFormat(WP2_ARGB_64) == WP2_STATUS_OK, quiet);
        reader = WP2::ImageReader(data, data_size, &buffer);
        status = reader.ReadFrame(&is_last, &duration_ms);
      }
      CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
          << "Got " << WP2GetStatusMessage(status) << " when reading frame "
          << image.size() << " of " << data_source;

      if (duration_ms == 0 && !is_last) {
        std::cout << "Warning: 0-second frame " << image.size() << " of "
                  << data_source << " was ignored" << std::endl;
        continue;
      }
      format = WP2FormatAtbpc(format, WP2Formatbpc(buffer.format()));
      CHECK_OR_RETURN(format != WP2_FORMAT_NUM, quiet);
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
    const WP2Status status =
        WP2::SaveImage(image.front().pixels, file_path, /*overwrite=*/true);
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
    CHECK_OR_RETURN(WP2Formatbpc(image.front().pixels.format()) == 8, quiet);
    ASSIGN_OR_RETURN(const Image bgra,
                     image.front().pixels.format() == WP2_BGRA_32
                         ? MakeView(image, quiet)
                         : CloneAs(image, WP2_BGRA_32, quiet));
    ASSIGN_OR_RETURN(const WP2::Data encoded_image,
                     GetCodecMetadata(Codec::kWebp).encode(input, bgra, quiet));
    std::ofstream(file_path, std::ios::binary)
        .write(reinterpret_cast<char*>(encoded_image.bytes),
               encoded_image.size);
  }
  return Status::kOk;
}

namespace {
class VectorWriter : public WP2::Writer {
 public:
  WP2_NO_DISCARD bool Reserve(size_t size) override {
    bytes_.reserve(bytes_.size() + size);
    return true;
  }
  WP2_NO_DISCARD bool Append(const void* data, size_t data_size) override {
    bytes_.insert(bytes_.end(), reinterpret_cast<const uint8_t*>(data),
                  reinterpret_cast<const uint8_t*>(data) + data_size);
    return true;
  }
  std::vector<uint8_t>&& TakeBytes() { return std::move(bytes_); }

 private:
  std::vector<uint8_t> bytes_;
};
}  // namespace

StatusOr<std::vector<uint8_t>> WriteStillImageOrAnimation(const Image& image,
                                                          bool quiet) {
  if (image.size() == 1) {
    VectorWriter writer;
    const WP2Status status = WP2::CompressPNG(image.front().pixels, &writer);
    CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
        << "WP2::CompressPNG() failed: " << WP2GetStatusMessage(status);
    return writer.TakeBytes();
  } else {
    const TaskInput input = {
        {Codec::kWebp, Subsampling::k444, 0, kQualityLossless},
        "undefined",
        ""};
    CHECK_OR_RETURN(WP2Formatbpc(image.front().pixels.format()) == 8, quiet);
    ASSIGN_OR_RETURN(const Image bgra,
                     image.front().pixels.format() == WP2_BGRA_32
                         ? MakeView(image, quiet)
                         : CloneAs(image, WP2_BGRA_32, quiet));
    ASSIGN_OR_RETURN(const WP2::Data encoded_image,
                     GetCodecMetadata(Codec::kWebp).encode(input, bgra, quiet));
    return std::vector<uint8_t>(encoded_image.bytes,
                                encoded_image.bytes + encoded_image.size);
  }
}

}  // namespace codec_compare_gen
