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

#include "src/codec_png.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/codec.h"
#include "src/frame.h"
#include "src/serialization.h"
#include "src/task.h"
#include "src/wp2/base.h"

#if defined(HAS_PNG)
#include <setjmp.h>

#include "png.h"
#include "pngconf.h"
#endif

namespace codec_compare_gen {
namespace {

std::string PngPrettyName(bool lossless, Subsampling subsampling, int effort) {
  return "PNG e" + std::to_string(effort) +
         SubsamplingToPrettyString(lossless, subsampling);
}

std::string PngVersion() {
#if defined(HAS_PNG)
  return PNG_LIBPNG_VER_STRING;
#else
  return "n/a";
#endif
}

std::vector<int> PngEfforts() {
  std::vector<int> efforts(10);
  std::iota(efforts.begin(), efforts.end(), 0);
  return efforts;
}

std::vector<int> PngLossyQualities() { return {}; }

#if defined(HAS_PNG)

void PNGAPI PngErrorFunction(png_structp png_ptr, png_const_charp error_msg) {
  const bool* const quiet =
      reinterpret_cast<const bool*>(png_get_error_ptr(png_ptr));
  if (quiet != nullptr && !*quiet && error_msg != nullptr) {
    std::cerr << "libpng error: " << error_msg << std::endl;
  }
  longjmp(png_jmpbuf(png_ptr), 1);
}

void PNGAPI PngWarningFunction(png_structp png_ptr,
                               png_const_charp warning_msg) {
  (void)png_ptr;
  (void)warning_msg;
}

void PNGAPI PngWriteData(png_structp png_ptr, png_bytep data,
                         png_size_t length) {
  auto* const out_vector =
      reinterpret_cast<std::vector<uint8_t>*>(png_get_io_ptr(png_ptr));
  out_vector->insert(out_vector->end(), data, data + length);
}

void PNGAPI PngFlushData(png_structp png_ptr) { (void)png_ptr; }

struct PngReadContext {
  const uint8_t* data;
  size_t size;
  size_t offset;
};

void PNGAPI PngReadData(png_structp png_ptr, png_bytep data,
                        png_size_t length) {
  auto* const context =
      reinterpret_cast<PngReadContext*>(png_get_io_ptr(png_ptr));
  if (context->offset + length > context->size) {
    png_error(png_ptr, "Read error: unexpected EOF");
    return;
  }
  std::memcpy(data, context->data + context->offset, length);
  context->offset += length;
}

StatusOr<WP2::Data> EncodePng(const TaskInput& input,
                              const Image& original_image, bool quiet) {
  CHECK_OR_RETURN(original_image.size() == 1, quiet)
      << "PNG does not support animation";
  const WP2::ArgbBuffer& pixels = original_image.front().pixels;
  CHECK_OR_RETURN(input.codec_settings.quality == kQualityLossless, quiet)
      << "PNG only supports lossless";
  CHECK_OR_RETURN(
      input.codec_settings.chroma_subsampling == Subsampling::kDefault ||
          input.codec_settings.chroma_subsampling == Subsampling::k444,
      quiet)
      << "PNG does not support chroma subsampling "
      << SubsamplingToString(input.codec_settings.chroma_subsampling);
  CHECK_OR_RETURN(
      input.codec_settings.effort >= 0 && input.codec_settings.effort <= 9,
      quiet)
      << "PNG effort " << input.codec_settings.effort
      << " must be between 0 and 9";

  const WP2SampleFormat format = pixels.format();
  int color_type;
  int bit_depth;
  if (format == WP2_RGB_24) {
    color_type = PNG_COLOR_TYPE_RGB;
    bit_depth = 8;
  } else if (format == WP2_RGBA_32) {
    color_type = PNG_COLOR_TYPE_RGBA;
    bit_depth = 8;
  } else if (format == WP2_RGB_48) {
    color_type = PNG_COLOR_TYPE_RGB;
    bit_depth = 16;
  } else if (format == WP2_RGBA_64) {
    color_type = PNG_COLOR_TYPE_RGBA;
    bit_depth = 16;
  } else {
    CHECK_OR_RETURN(false, quiet)
        << "Unsupported format for PNG: " << static_cast<int>(format);
  }

  std::vector<uint8_t> out_bytes;
  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, const_cast<bool*>(&quiet),
                              PngErrorFunction, PngWarningFunction);
  CHECK_OR_RETURN(png != nullptr, quiet) << "png_create_write_struct() failed";
  png_infop info = png_create_info_struct(png);
  if (info == nullptr) {
    png_destroy_write_struct(&png, nullptr);
    CHECK_OR_RETURN(false, quiet) << "png_create_info_struct() failed";
  }
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    CHECK_OR_RETURN(false, quiet) << "libpng write error";
  }

  png_set_write_fn(png, &out_bytes, PngWriteData, PngFlushData);
  png_set_compression_level(png, input.codec_settings.effort);
  png_set_IHDR(png, info, pixels.width(), pixels.height(), bit_depth,
               color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  if (bit_depth == 16) {
    png_set_swap(png);  // Expect little-endian.
  }
  for (uint32_t y = 0; y < pixels.height(); ++y) {
    png_bytep row = const_cast<png_bytep>(
        reinterpret_cast<const png_byte*>(pixels.GetRow(y)));
    png_write_rows(png, &row, 1);
  }
  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);

  WP2::Data data;
  CHECK_OR_RETURN(
      data.CopyFrom(out_bytes.data(), out_bytes.size()) == WP2_STATUS_OK,
      quiet);
  return data;
}

Status DecodePngImpl(const TaskInput&, const WP2::Data& encoded_image,
                     Image& image, std::vector<png_bytep>& row_pointers,
                     bool quiet) {
  CHECK_OR_RETURN(encoded_image.bytes != nullptr && encoded_image.size > 0,
                  quiet);

  PngReadContext context = {encoded_image.bytes, encoded_image.size, 0};
  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, const_cast<bool*>(&quiet),
                             PngErrorFunction, PngWarningFunction);
  CHECK_OR_RETURN(png != nullptr, quiet) << "png_create_read_struct() failed";
  png_infop info = png_create_info_struct(png);
  if (info == nullptr) {
    png_destroy_read_struct(&png, nullptr, nullptr);
    CHECK_OR_RETURN(false, quiet) << "png_create_info_struct() failed";
  }
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_read_struct(&png, &info, nullptr);
    CHECK_OR_RETURN(false, quiet) << "libpng read error";
  }

  png_set_read_fn(png, &context, PngReadData);
  png_read_info(png, info);

  png_uint_32 width, height;
  int bit_depth, color_type;
  png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, nullptr,
               nullptr, nullptr);

  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(png);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    png_set_expand_gray_1_2_4_to_8(png);
  }
  if (png_get_valid(png, info, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png);
  }
  if (bit_depth == 16) {
    png_set_swap(png);
  }
  (void)png_set_interlace_handling(png);
  png_read_update_info(png, info);

  const int updated_color_type = png_get_color_type(png, info);
  const int updated_bit_depth = png_get_bit_depth(png, info);
  const bool has_alpha = (updated_color_type & PNG_COLOR_MASK_ALPHA);

  WP2SampleFormat format;
  if (updated_bit_depth == 16) {
    format = has_alpha ? WP2_RGBA_64 : WP2_RGB_48;
  } else if (updated_bit_depth == 8) {
    format = has_alpha ? WP2_RGBA_32 : WP2_RGB_24;
  } else {
    png_destroy_read_struct(&png, &info, nullptr);
    CHECK_OR_RETURN(false, quiet)
        << "Unsupported decoded bit depth: " << updated_bit_depth;
  }

  image.reserve(1);
  image.emplace_back(WP2::ArgbBuffer(format), /*duration_ms=*/0);
  if (image.back().pixels.Resize(width, height) != WP2_STATUS_OK) {
    png_destroy_read_struct(&png, &info, nullptr);
    CHECK_OR_RETURN(false, quiet) << "Failed to resize ArgbBuffer";
  }

  row_pointers.reserve(height);
  for (png_uint_32 y = 0; y < height; ++y) {
    row_pointers.push_back(
        reinterpret_cast<png_bytep>(image.back().pixels.GetRow(y)));
  }
  png_read_image(png, row_pointers.data());
  png_read_end(png, nullptr);
  png_destroy_read_struct(&png, &info, nullptr);

  return Status::kOk;
}

StatusOr<std::pair<Image, double>> DecodePng(const TaskInput& input,
                                             const WP2::Data& encoded_image,
                                             bool quiet) {
  Image image;
  std::vector<png_bytep> row_pointers;
  OK_OR_RETURN(DecodePngImpl(input, encoded_image, image, row_pointers, quiet));
  return std::pair<Image, double>(std::move(image), 0.0);
}

#else

StatusOr<WP2::Data> EncodePng(const TaskInput&, const Image&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_PNG";
}

StatusOr<std::pair<Image, double>> DecodePng(const TaskInput&, const WP2::Data&,
                                             bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_PNG";
}

#endif  // HAS_PNG

}  // namespace

CodecMetadata GetPngMetadata() {
  return CodecMetadata{
      "png",
      PngPrettyName,
      PngVersion,
      " -DCCGEN_ENABLE_AVIF=OFF -DCCGEN_ENABLE_JPEG=OFF -DCCGEN_ENABLE_PNG=ON",
      PngEfforts,
      PngLossyQualities,
      /*extension=*/"png",
      /*mime_type=*/"image/png",
      /*is_supported_by_browsers=*/true,
      /*supports_16bit=*/true,
      /*opaque_format=*/WP2_RGB_24,
      /*transparent_format=*/WP2_RGBA_32,
      EncodePng,
      DecodePng,
  };
}

}  // namespace codec_compare_gen
