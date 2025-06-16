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

#include "src/codec_basis.h"

#include <cstdint>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/frame.h"
#include "src/serialization.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

#if defined(HAS_BASIS)
#include "third_party/basis_universal/encoder/basisu_comp.h"
#include "third_party/basis_universal/encoder/basisu_enc.h"
#include "third_party/basis_universal/transcoder/basisu.h"
#include "third_party/basis_universal/transcoder/basisu_transcoder.h"
#endif

namespace codec_compare_gen {

std::string BasisVersion() {
#if defined(HAS_BASIS)
  return BASISU_LIB_VERSION_STRING;
#else
  return "n/a";
#endif
}

BasisContext::BasisContext(bool enabled) : enabled(enabled) {
#if defined(HAS_BASIS)
  if (enabled) {
    basisu::basisu_encoder_init();
    // Uncomment for debugging.
    // basisu::enable_debug_printf(true);
  }
#endif
}
BasisContext::~BasisContext() {
#if defined(HAS_BASIS)
  if (enabled) basisu::basisu_encoder_deinit();
#endif
}

std::vector<int> BasisLossyQualities() {
  std::vector<int> qualities(basisu::BASISU_QUALITY_MAX -
                             basisu::BASISU_QUALITY_MIN + 1);
  std::iota(qualities.begin(), qualities.end(), basisu::BASISU_QUALITY_MIN);
  return qualities;
}

#if defined(HAS_WEBP2)

#if defined(HAS_BASIS)

StatusOr<WP2::Data> EncodeBasis(const TaskInput& input,
                                const Image& original_image, bool quiet) {
  CHECK_OR_RETURN(original_image.size() == 1, quiet);
  const WP2::ArgbBuffer& pixels = original_image.front().pixels;
  CHECK_OR_RETURN(input.codec_settings.effort == 0, quiet);
  // Lossless does not seem supported.
  CHECK_OR_RETURN(input.codec_settings.quality != kQualityLossless, quiet);
  CHECK_OR_RETURN(
      pixels.format() == WP2_RGB_24 || pixels.format() == WP2_RGBA_32, quiet);
  CHECK_OR_RETURN(
      input.codec_settings.chroma_subsampling == Subsampling::kDefault ||
          input.codec_settings.chroma_subsampling == Subsampling::k444,
      quiet)
      << "basis does not support chroma subsampling "
      << SubsamplingToString(input.codec_settings.chroma_subsampling);

  // basisu::basisu_encoder_init() must have been called. See BasisContext.

  basisu::basis_compressor_params params;

  const uint32_t num_channels = WP2FormatNumChannels(pixels.format());
  CHECK_OR_RETURN(pixels.stride() == pixels.width() * num_channels, quiet);
  params.m_source_images.resize(1);
  params.m_source_images.back().init(pixels.GetRow8(0), pixels.width(),
                                     pixels.height(), num_channels);
  params.m_etc1s_quality_level = input.codec_settings.quality;
  params.m_mip_gen = false;
  params.m_multithreading = false;

  // There must be at least one thread on top of the calling thread apparently.
  basisu::job_pool job_pool(/*num_threads=*/1);
  params.m_pJob_pool = &job_pool;

  // Uncomment for debugging.
  // params.m_debug = true;
  // params.m_status_output = true;

  basisu::basis_compressor basisCompressor;

  CHECK_OR_RETURN(basisCompressor.init(params), quiet);
  basisu::basis_compressor::error_code result = basisCompressor.process();
  CHECK_OR_RETURN(result == basisu::basis_compressor::cECSuccess, quiet)
      << "basisCompressor.process() failed with " << result;

  // Basis files are padded to multiples of 4 pixels in both dimensions.
  // Store the padding amount in the file header to crop it at decoding.
  const uint32_t hpad = pixels.width() & 3, vpad = pixels.height() & 3;
  const uint8_t pad = (hpad << 2) | (vpad << 0);
  WP2::Data data;
  CHECK_OR_RETURN(data.Append(&pad, 1) == WP2_STATUS_OK, quiet);
  CHECK_OR_RETURN(data.Append(basisCompressor.get_output_basis_file().data(),
                              basisCompressor.get_output_basis_file().size()) ==
                      WP2_STATUS_OK,
                  quiet);
  return data;
}

StatusOr<std::pair<Image, double>> DecodeBasis(const TaskInput& input,
                                               const WP2::Data& encoded_image,
                                               bool quiet) {
  CHECK_OR_RETURN(encoded_image.size > 1, quiet);
  const uint8_t pad = encoded_image.bytes[0];
  const uint32_t hpad = ((pad >> 2) & 3), vpad = ((pad >> 0) & 3);
  const uint8_t* bytes = encoded_image.bytes + 1;
  const uint32_t size = encoded_image.size - 1;

  // basist::basisu_transcoder_init() was already called by
  // basisu::basisu_encoder_init().

  basist::basisu_transcoder transcoder;
  CHECK_OR_RETURN(transcoder.validate_header(bytes, size), quiet);
  basist::basisu_file_info fileinfo;
  CHECK_OR_RETURN(transcoder.get_file_info(bytes, size, fileinfo), quiet);
  CHECK_OR_RETURN(transcoder.start_transcoding(bytes, size), quiet);
  CHECK_OR_RETURN(transcoder.get_total_images(bytes, size) == 1, quiet);
  basist::basisu_image_info image_info;
  CHECK_OR_RETURN(
      transcoder.get_image_info(bytes, size, image_info, /*image_index=*/0),
      quiet);
  CHECK_OR_RETURN(image_info.m_total_levels == 1, quiet);

  Image image;
  image.reserve(1);
  image.emplace_back(WP2::ArgbBuffer(WP2_RGBA_32), /*duration_ms=*/0);
  CHECK_OR_RETURN(image.back().pixels.Resize(
                      image_info.m_width, image_info.m_height) == WP2_STATUS_OK,
                  quiet);

  CHECK_OR_RETURN(
      transcoder.transcode_image_level(
          bytes, size, /*image_index=*/0, /*level_index=*/0,
          image.back().pixels.GetRow8(0),
          image.back().pixels.height() * image.back().pixels.width(),
          basist::transcoder_texture_format::cTFRGBA32),
      quiet);

  // Basis files are padded to multiples of 4 pixels in both dimensions.
  // Retrieve the original image dimensions.
  const WP2::Rectangle original_rect = {
      0, 0, image.back().pixels.width() - (hpad == 0 ? 0 : 4 - hpad),
      image.back().pixels.height() - (vpad == 0 ? 0 : 4 - vpad)};
  WP2::ArgbBuffer crop(image.back().pixels.format());
  CHECK_OR_RETURN(
      crop.SetView(image.back().pixels, original_rect) == WP2_STATUS_OK, quiet);
  WP2::ArgbBuffer final(crop.HasTransparency() ? WP2_RGBA_32 : WP2_RGB_24);
  if (image.back().pixels.width() == original_rect.width &&
      image.back().pixels.height() == original_rect.height &&
      image.back().pixels.format() == final.format()) {
    // No copy needed.
  } else {
    CHECK_OR_RETURN(final.ConvertFrom(crop) == WP2_STATUS_OK, quiet);
    swap(image.back().pixels, final);
  }
  return std::pair<Image, double>(std::move(image), 0);
}

#else
StatusOr<WP2::Data> EncodeBasis(const TaskInput&, const Image&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_BASIS";
}
StatusOr<std::pair<Image, double>> DecodeBasis(const TaskInput&,
                                               const WP2::Data&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_BASIS";
}
#endif  // HAS_BASIS

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
