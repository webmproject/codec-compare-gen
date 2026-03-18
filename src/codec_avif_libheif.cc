// Copyright 2025 Google LLC
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

#include "src/codec_avif_libheif.h"

#if defined(HAS_HEIF)
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#endif

#include <cassert>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/frame.h"
#include "src/task.h"
#include "src/timer.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

#if defined(HAS_HEIF)
#include "libheif/heif_aux_images.h"
#include "libheif/heif_context.h"
#include "libheif/heif_decoding.h"
#include "libheif/heif_encoding.h"
#include "libheif/heif_error.h"
#include "libheif/heif_image.h"
#include "libheif/heif_image_handle.h"
#include "libheif/heif_library.h"
#endif

namespace codec_compare_gen {

std::string AvifLibheifVersion() {
#if defined(HAS_HEIF)
  return heif_get_version();
#else
  return "n/a";
#endif
}

std::vector<int> AvifLibheifLossyQualities() {
  std::vector<int> qualities(64);
  for (int i = 0; i < qualities.size(); ++i) {
    // Assuming the same mapping as libavif's avifQualityToQuantizer():
    //   quantizer = ((100 - quality) * 63 + 50) / 100;
    qualities[i] = ((63 - i) * 100 + 63 / 2) / 63;
  }
  return qualities;  // [0:63] (63 is lossless but in YUV so RGB is lossy).
}

#if defined(HAS_WEBP2)

#if defined(HAS_HEIF)

namespace {

// heif_cxx.h already provides a C++ API but it relies on throwing exceptions.
using HeifImage = std::unique_ptr<heif_image, decltype(&heif_image_release)>;
using HeifContext = std::unique_ptr<heif_context, decltype(&heif_context_free)>;
using HeifEncoder =
    std::unique_ptr<heif_encoder, decltype(&heif_encoder_release)>;
using HeifImageHandle =
    std::unique_ptr<heif_image_handle, decltype(&heif_image_handle_release)>;
using HeifDecodingOptions =
    std::unique_ptr<heif_decoding_options,
                    decltype(&heif_decoding_options_free)>;

Status ArgbBufferToHeifImage(const WP2::ArgbBuffer& wp2_image,
                             HeifImage* image_ptr, bool quiet) {
  heif_chroma chroma;
  if (wp2_image.format() == WP2_RGB_24) {
    chroma = heif_chroma_interleaved_RGB;
  } else {
    // TODO: b/451945988 - Support 16-bit.
    CHECK_OR_RETURN(wp2_image.format() == WP2_RGBA_32, quiet)
        << "Unsupported format: " << wp2_image.format();
    chroma = heif_chroma_interleaved_RGBA;
  }

  heif_image* image = nullptr;
  heif_image_create(static_cast<int>(wp2_image.width()),
                    static_cast<int>(wp2_image.height()), heif_colorspace_RGB,
                    chroma, &image);
  CHECK_OR_RETURN(image != nullptr, quiet) << "heif_image_create failed";
  image_ptr->reset(image);

  const heif_channel channel = heif_channel_interleaved;
  const heif_error err = heif_image_add_plane(
      image, channel, static_cast<int>(wp2_image.width()),
      static_cast<int>(wp2_image.height()), WP2Formatbpc(wp2_image.format()));
  CHECK_OR_RETURN(err.code == heif_error_Ok, quiet)
      << "heif_image_add_plane failed: " << err.message;

  size_t stride;
  uint8_t* data = heif_image_get_plane2(image, channel, &stride);
  CHECK_OR_RETURN(data != nullptr, quiet) << "heif_image_get_plane failed";
  for (uint32_t y = 0; y < wp2_image.height(); ++y) {
    const uint8_t* row = reinterpret_cast<const uint8_t*>(wp2_image.GetRow(y));
    std::copy(row, row + wp2_image.width() * WP2FormatBpp(wp2_image.format()),
              data);
    data += stride;
  }
  return Status::kOk;
}

heif_error writer_callback(heif_context* ctx, const void* data, size_t size,
                           void* userdata) {
  WP2::Data* writer_ctx = reinterpret_cast<WP2::Data*>(userdata);
  const WP2Status status =
      writer_ctx->Append(reinterpret_cast<const uint8_t*>(data), size);
  return {status == WP2_STATUS_OK ? heif_error_Ok
                                  : heif_error_Memory_allocation_error};
}

}  // namespace

StatusOr<WP2::Data> EncodeAvifLibheif(const TaskInput& input,
                                      const Image& original_image, bool quiet) {
  const bool lossless = input.codec_settings.quality == kQualityLossless;
  heif_context* context = heif_context_alloc();
  CHECK_OR_RETURN(context != nullptr, quiet) << "heif_context_alloc failed";
  const HeifContext context_ptr(context, &heif_context_free);

  heif_encoder* encoder = nullptr;
  heif_error err = heif_context_get_encoder_for_format(
      context_ptr.get(), heif_compression_AV1, &encoder);
  CHECK_OR_RETURN(err.code == heif_error_Ok && encoder != nullptr, quiet)
      << "heif_context_get_encoder_for_format failed for AV1: " << err.message;
  const HeifEncoder encoder_ptr(encoder, &heif_encoder_release);

  err = heif_encoder_set_lossless(encoder, lossless ? 1 : 0);
  CHECK_OR_RETURN(err.code == heif_error_Ok, quiet)
      << "heif_encoder_set_lossless failed: " << err.message;
  if (lossless) {
    CHECK_OR_RETURN(
        input.codec_settings.chroma_subsampling == Subsampling::kDefault ||
            input.codec_settings.chroma_subsampling == Subsampling::k444,
        quiet);
  } else {
    // Default seems to be full-range YUV 4:2:0 with CICP set to 1/13/6.
    // TODO: b/451945988 - Support lossy 4:4:4.
    CHECK_OR_RETURN(
        input.codec_settings.chroma_subsampling == Subsampling::kDefault ||
            input.codec_settings.chroma_subsampling == Subsampling::k420,
        quiet);
    err = heif_encoder_set_lossy_quality(encoder, input.codec_settings.quality);
    CHECK_OR_RETURN(err.code == heif_error_Ok, quiet)
        << "heif_encoder_set_lossy_quality failed: " << err.message;
  }

  err = heif_encoder_set_parameter_integer(encoder, "speed",
                                           input.codec_settings.effort);
  CHECK_OR_RETURN(err.code == heif_error_Ok, quiet)
      << "heif_encoder_set_param for speed failed: " << err.message;

  err = heif_encoder_set_parameter_integer(encoder, "threads", 1);
  CHECK_OR_RETURN(err.code == heif_error_Ok, quiet)
      << "heif_encoder_set_param for threads failed: " << err.message;

  // TODO: b/451945988 - Support sequences.
  CHECK_OR_RETURN(original_image.size() == 1, quiet);
  HeifImage image(nullptr, &heif_image_release);
  OK_OR_RETURN(
      ArgbBufferToHeifImage(original_image.front().pixels, &image, quiet));
  err = heif_context_encode_image(context, image.get(), encoder, nullptr,
                                  nullptr);
  CHECK_OR_RETURN(err.code == heif_error_Ok, quiet)
      << "heif_context_encode_image failed: " << err.message;

  WP2::Data encoded_image;
  struct heif_writer writer;
  writer.writer_api_version = 1;
  writer.write = writer_callback;

  err = heif_context_write(context, &writer, &encoded_image);
  CHECK_OR_RETURN(err.code == heif_error_Ok, quiet)
      << "heif_context_write failed: " << err.message;

  return encoded_image;
}

StatusOr<std::pair<Image, double>> DecodeAvifLibheif(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  heif_context* context = heif_context_alloc();
  CHECK_OR_RETURN(context != nullptr, quiet) << "heif_context_alloc failed";
  const HeifContext context_ptr(context, &heif_context_free);

  heif_context_set_max_decoding_threads(context, 0);

  heif_error err = heif_context_read_from_memory_without_copy(
      context, encoded_image.bytes, encoded_image.size, nullptr);
  CHECK_OR_RETURN(err.code == heif_error_Ok, quiet)
      << "heif_context_read_from_memory failed: " << err.message;

  heif_item_id id;
  err = heif_context_get_primary_image_ID(context, &id);
  CHECK_OR_RETURN(err.code == heif_error_Ok, quiet)
      << "heif_context_get_primary_image_ID failed: " << err.message;

  heif_image_handle* handle = nullptr;
  err = heif_context_get_image_handle(context, id, &handle);
  CHECK_OR_RETURN(err.code == heif_error_Ok && handle != nullptr, quiet)
      << "heif_context_get_image_handle failed: " << err.message;
  const HeifImageHandle handle_ptr(handle, &heif_image_handle_release);

  const bool has_alpha = heif_image_handle_has_alpha_channel(handle);
  const heif_chroma chroma =
      has_alpha ? heif_chroma_interleaved_RGBA : heif_chroma_interleaved_RGB;

  heif_decoding_options* options = heif_decoding_options_alloc();
  CHECK_OR_RETURN(options != nullptr, quiet)
      << "heif_decoding_options_alloc failed";
  const HeifDecodingOptions options_ptr(options, &heif_decoding_options_free);
  options->num_codec_threads = 1;
  options->ignore_transformations = true;  // There should be no transformation.

  heif_image* decoded = nullptr;
  err =
      heif_decode_image(handle, &decoded, heif_colorspace_RGB, chroma, options);
  CHECK_OR_RETURN(err.code == heif_error_Ok && decoded != nullptr, quiet)
      << "heif_decode_image failed: " << err.message;
  const HeifImage decoded_ptr(decoded, &heif_image_release);

  const uint32_t width = static_cast<uint32_t>(
      heif_image_get_width(decoded, heif_channel_interleaved));
  const uint32_t height = static_cast<uint32_t>(
      heif_image_get_height(decoded, heif_channel_interleaved));
  size_t stride;
  const uint8_t* data = heif_image_get_plane_readonly2(
      decoded, heif_channel_interleaved, &stride);
  CHECK_OR_RETURN(data != nullptr, quiet)
      << "heif_image_get_plane_readonly failed";

  Image image;
  image.reserve(1);
  image.emplace_back(WP2::ArgbBuffer(has_alpha ? WP2_RGBA_32 : WP2_RGB_24),
                     /*duration_ms=*/0);
  CHECK_OR_RETURN(image.back().pixels.Resize(width, height) == WP2_STATUS_OK,
                  quiet);
  for (uint32_t y = 0; y < height; ++y) {
    std::copy(data, data + width * WP2FormatBpp(image.back().pixels.format()),
              reinterpret_cast<uint8_t*>(image.back().pixels.GetRow(y)));
    data += stride;
  }

  return std::pair<Image, double>(std::move(image), 0);
}

#else
StatusOr<WP2::Data> EncodeAvifLibheif(const TaskInput&, const Image&,
                                      bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_HEIF";
}
StatusOr<std::pair<Image, double>> DecodeAvifLibheif(const TaskInput&,
                                                     const WP2::Data&,
                                                     bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_HEIF";
}
#endif  // HAS_HEIF

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
