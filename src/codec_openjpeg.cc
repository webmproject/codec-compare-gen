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

#include "src/codec_openjpeg.h"

#if defined(HAS_OPENJPEG) && defined(HAS_WEBP2)
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#endif
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/frame.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

#if defined(HAS_OPENJPEG)
#include "openjpeg.h"
#endif

namespace codec_compare_gen {

std::string OpenjpegVersion() {
#if defined(HAS_OPENJPEG)
  return opj_version();
#else
  return "n/a";
#endif
}

std::vector<int> OpenjpegLossyQualities() {
  std::vector<int> qualities(101);
  std::iota(qualities.begin(), qualities.end(), 0);
  return qualities;
}

#if defined(HAS_WEBP2)

#if defined(HAS_OPENJPEG)

StatusOr<WP2::Data> EncodeOpenjpeg(const TaskInput& input,
                                   const Image& original_image, bool quiet) {
  CHECK_OR_RETURN(original_image.size() == 1, quiet);
  const WP2::ArgbBuffer& pixels = original_image.front().pixels;
  // OpenJPEG has no effort parameter.
  CHECK_OR_RETURN(input.codec_settings.effort == 0, quiet);

  const uint32_t num_channels = WP2FormatNumChannels(pixels.format());
  uint32_t alpha_channel_index = 0;
  WP2FormatHasAlpha(pixels.format(), &alpha_channel_index);

  opj_cparameters parameters;
  opj_set_default_encoder_parameters(&parameters);
  // Match the default settings of opj_compress.
  parameters.tcp_numlayers = 1;
  parameters.cp_disto_alloc = 1;
  parameters.tcp_mct = 1;    // RGB->YCC. TODO(yguyon): Try keeping it to 0.
  uint32_t subsampling = 1;  // 1 for 4:4:4, 2 for 4:2:0.
  if (input.codec_settings.quality == kQualityLossless) {
    CHECK_OR_RETURN(
        input.codec_settings.chroma_subsampling == Subsampling::k444 ||
            input.codec_settings.chroma_subsampling == Subsampling::kDefault,
        quiet);
    parameters.tcp_rates[0] = 0.f;  // Equivalent to 1.f (lossless).
  } else {
    if (input.codec_settings.chroma_subsampling == Subsampling::k420 ||
        input.codec_settings.chroma_subsampling == Subsampling::kDefault) {
      // Note that opj_cparameters::subsampling_dx/y are ignored by opj_encode.
      subsampling = 2;
    }
    parameters.tcp_rates[0] = 101.f - input.codec_settings.quality;
    // parameters.irreversible = 1;  // TODO(yguyon): Try this.
  }

  // Adapt numresolution to image dimensions to avoid encoding failures. See
  // https://github.com/uclouvain/openjpeg/blob/e7453e398b110891778d8da19209792c69ca7169/src/lib/openjp2/j2k.c#L8840-L8852.
  // and https://github.com/uclouvain/openjpeg/issues/215.
  // Keep the default value 6 if possible.
  parameters.numresolution =
      std::min(parameters.numresolution,
               1 + static_cast<int>(std::floor(
                       std::log2(std::min(pixels.width(), pixels.height())))));
  // Note that smaller values of numresolution for lossless compression can lead
  // to significantly smaller files in some cases.
  // TODO(yguyon): Try all values and keep the smallest file?

  CHECK_OR_RETURN(num_channels <= 4, quiet);
  opj_image_cmptparm_t compparams[4]{};
  for (uint32_t i = 0; i < num_channels; ++i) {
    compparams[i].prec = i == alpha_channel_index
                             ? WP2Formatbpalpha(pixels.format())
                             : WP2Formatbpc(pixels.format());
    compparams[i].sgnd = 0;
    compparams[i].dx = subsampling;
    compparams[i].dy = subsampling;
    compparams[i].w = pixels.width();
    compparams[i].h = pixels.height();
  }

  const auto colorspace =
      (num_channels > 2) ? OPJ_CLRSPC_SRGB : OPJ_CLRSPC_GRAY;
  std::unique_ptr<opj_image_t, decltype(&opj_image_destroy)> opj_image(
      opj_image_create(num_channels, compparams, colorspace),
      opj_image_destroy);
  CHECK_OR_RETURN(opj_image != nullptr, quiet);

  opj_image->x0 = 0;
  opj_image->y0 = 0;
  // Taken from
  // https://github.com/uclouvain/openjpeg/blob/e7453e398b110891778d8da19209792c69ca7169/src/bin/jp2/convert.c#L1890-L1893
  opj_image->x1 = (compparams[0].w - 1) * compparams[0].dx + 1;
  opj_image->y1 = (compparams[0].h - 1) * compparams[0].dy + 1;
  for (uint32_t i = 0; i < num_channels; ++i) {
    opj_image->comps[i].alpha = i == alpha_channel_index ? 1 : 0;
    for (uint32_t y = 0; y < pixels.height(); ++y) {
      OPJ_INT32* const opj_row = opj_image->comps[i].data + y * pixels.width();
      if (WP2Formatbpc(pixels.format()) == 8) {
        const uint8_t* const row = pixels.GetRow8(y);
        for (uint32_t x = 0; x < pixels.width(); ++x) {
          opj_row[x] = row[num_channels * x + i];
        }
      } else {
        const uint16_t* const row = pixels.GetRow16(y);
        for (uint32_t x = 0; x < pixels.width(); ++x) {
          opj_row[x] = row[num_channels * x + i];
        }
      }
    }
  }

  // OPJ_CODEC_J2K leads to smaller files but only OPJ_CODEC_JP2 supports alpha
  // channel tagging.
  std::unique_ptr<opj_codec_t, decltype(&opj_destroy_codec)> codec(
      opj_create_compress(OPJ_CODEC_JP2), opj_destroy_codec);
  CHECK_OR_RETURN(codec != nullptr, quiet);

  std::string error_or_warning;
  const auto log = [](const char* message, void* error_or_warning) {
    std::cerr << message << std::endl;
    *reinterpret_cast<std::string*>(error_or_warning) = message;
  };
  CHECK_OR_RETURN(opj_set_error_handler(codec.get(), log, &error_or_warning),
                  quiet);
  CHECK_OR_RETURN(opj_set_warning_handler(codec.get(), log, &error_or_warning),
                  quiet);

  CHECK_OR_RETURN(opj_setup_encoder(codec.get(), &parameters, opj_image.get()),
                  quiet);

  struct Stream {
    WP2::Data encoded_image;
    size_t offset;
  } stream_data{};
  std::unique_ptr<opj_stream_t, decltype(&opj_stream_destroy)> stream(
      opj_stream_default_create(OPJ_STREAM_WRITE), opj_stream_destroy);
  CHECK_OR_RETURN(stream != nullptr, quiet);
  opj_stream_set_user_data(stream.get(), &stream_data, [](void* p_user_data) {
    *reinterpret_cast<Stream*>(p_user_data) = {};
  });
  opj_stream_set_write_function(
      stream.get(),
      [](void* p_buffer, OPJ_SIZE_T p_nb_bytes,
         void* p_user_data) -> OPJ_SIZE_T {
        Stream& stream = *reinterpret_cast<Stream*>(p_user_data);
        assert(stream.offset <= stream.encoded_image.size);
        if (p_nb_bytes >
            std::numeric_limits<OPJ_SIZE_T>::max() - stream.offset) {
          return static_cast<OPJ_SIZE_T>(-1);
        }
        if (stream.offset + p_nb_bytes > stream.encoded_image.size &&
            stream.encoded_image.Resize(stream.offset + p_nb_bytes,
                                        /*keep_bytes=*/true) != WP2_STATUS_OK) {
          return static_cast<OPJ_SIZE_T>(-1);
        }
        std::memcpy(stream.encoded_image.bytes + stream.offset, p_buffer,
                    p_nb_bytes);
        stream.offset += p_nb_bytes;
        return p_nb_bytes;
      });
  opj_stream_set_seek_function(
      stream.get(), [](OPJ_OFF_T p_nb_bytes, void* p_user_data) -> OPJ_BOOL {
        Stream& stream = *reinterpret_cast<Stream*>(p_user_data);
        assert(stream.offset <= stream.encoded_image.size);
        if (p_nb_bytes < 0) return OPJ_FALSE;
        const size_t new_size = static_cast<size_t>(p_nb_bytes);
        const size_t previous_size = stream.encoded_image.size;
        if (new_size > previous_size) {
          if (stream.encoded_image.Resize(new_size, /*keep_bytes=*/true) !=
              WP2_STATUS_OK) {
            return OPJ_FALSE;
          }
          std::fill(stream.encoded_image.bytes + previous_size,
                    stream.encoded_image.bytes + new_size, 0);
        }
        stream.offset = new_size;
        return OPJ_TRUE;
      });
  opj_stream_set_skip_function(
      stream.get(), [](OPJ_OFF_T p_nb_bytes, void* p_user_data) -> OPJ_OFF_T {
        Stream& stream = *reinterpret_cast<Stream*>(p_user_data);
        assert(stream.offset <= stream.encoded_image.size);
        if (p_nb_bytes < 0 ||
            static_cast<size_t>(p_nb_bytes) >
                std::numeric_limits<OPJ_SIZE_T>::max() - stream.offset) {
          return OPJ_OFF_T{-1};
        }
        const size_t new_size = stream.offset + static_cast<size_t>(p_nb_bytes);
        const size_t previous_size = stream.encoded_image.size;
        if (new_size > previous_size) {
          if (stream.encoded_image.Resize(new_size, /*keep_bytes=*/true) !=
              WP2_STATUS_OK) {
            return OPJ_OFF_T{-1};
          }
          std::fill(stream.encoded_image.bytes + previous_size,
                    stream.encoded_image.bytes + new_size, 0);
        }
        stream.offset = new_size;
        return p_nb_bytes;
      });

  CHECK_OR_RETURN(
      opj_start_compress(codec.get(), opj_image.get(), stream.get()), quiet);
  CHECK_OR_RETURN(opj_encode(codec.get(), stream.get()), quiet);
  CHECK_OR_RETURN(opj_end_compress(codec.get(), stream.get()), quiet);

  if (!error_or_warning.empty()) {
    return Status::kUnknownError;
  }
  return std::move(stream_data.encoded_image);
}

StatusOr<std::pair<Image, double>> DecodeOpenjpeg(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  struct Stream {
    const WP2::Data& encoded_image;
    size_t offset;
  } stream_data{encoded_image, 0};
  std::unique_ptr<opj_stream_t, decltype(&opj_stream_destroy)> stream(
      opj_stream_default_create(OPJ_STREAM_READ), opj_stream_destroy);
  CHECK_OR_RETURN(stream != nullptr, quiet);
  opj_stream_set_user_data(stream.get(), &stream_data, [](void*) {});
  opj_stream_set_user_data_length(stream.get(), encoded_image.size);
  opj_stream_set_read_function(
      stream.get(),
      [](void* p_buffer, OPJ_SIZE_T p_nb_bytes,
         void* p_user_data) -> OPJ_SIZE_T {
        Stream& stream = *reinterpret_cast<Stream*>(p_user_data);
        assert(stream.offset <= stream.encoded_image.size);
        if (stream.offset >= stream.encoded_image.size) {
          if (p_nb_bytes == 0) return p_nb_bytes;
          return static_cast<OPJ_SIZE_T>(-1);  // End of stream.
        }
        if (p_nb_bytes > stream.encoded_image.size - stream.offset) {
          // Return what is available instead of an error.
          p_nb_bytes = stream.encoded_image.size - stream.offset;
        }
        std::memcpy(p_buffer, stream.encoded_image.bytes + stream.offset,
                    p_nb_bytes);
        stream.offset += p_nb_bytes;
        return p_nb_bytes;
      });
  opj_stream_set_seek_function(
      stream.get(), [](OPJ_OFF_T p_nb_bytes, void* p_user_data) -> OPJ_BOOL {
        Stream& stream = *reinterpret_cast<Stream*>(p_user_data);
        assert(stream.offset <= stream.encoded_image.size);
        if (p_nb_bytes < 0 ||
            static_cast<size_t>(p_nb_bytes) > stream.encoded_image.size) {
          return OPJ_FALSE;
        }
        stream.offset = static_cast<size_t>(p_nb_bytes);
        return OPJ_TRUE;
      });
  opj_stream_set_skip_function(
      stream.get(), [](OPJ_OFF_T p_nb_bytes, void* p_user_data) -> OPJ_OFF_T {
        Stream& stream = *reinterpret_cast<Stream*>(p_user_data);
        assert(stream.offset <= stream.encoded_image.size);
        if (p_nb_bytes < 0 || static_cast<size_t>(p_nb_bytes) >
                                  stream.encoded_image.size - stream.offset) {
          return OPJ_OFF_T{-1};
        }
        stream.offset += static_cast<size_t>(p_nb_bytes);
        return p_nb_bytes;
      });

  std::unique_ptr<opj_codec_t, decltype(&opj_destroy_codec)> codec(
      opj_create_decompress(OPJ_CODEC_JP2), opj_destroy_codec);
  CHECK_OR_RETURN(codec != nullptr, quiet);

  std::string error_or_warning;
  const auto log = [](const char* message, void* error_or_warning) {
    std::cerr << message << std::endl;
    *reinterpret_cast<std::string*>(error_or_warning) = message;
  };
  CHECK_OR_RETURN(opj_set_error_handler(codec.get(), log, &error_or_warning),
                  quiet);
  CHECK_OR_RETURN(opj_set_warning_handler(codec.get(), log, &error_or_warning),
                  quiet);

  opj_dparameters parameters;
  opj_set_default_decoder_parameters(&parameters);
  CHECK_OR_RETURN(opj_setup_decoder(codec.get(), &parameters), quiet);

  opj_image_t* opj_image_ptr;
  CHECK_OR_RETURN(opj_read_header(stream.get(), codec.get(), &opj_image_ptr),
                  quiet);
  CHECK_OR_RETURN(opj_image_ptr != nullptr, quiet);
  std::unique_ptr<opj_image_t, decltype(&opj_image_destroy)> opj_image(
      opj_image_ptr, opj_image_destroy);

  CHECK_OR_RETURN(opj_decode(codec.get(), stream.get(), opj_image.get()),
                  quiet);

  uint32_t alpha_channel_index = opj_image->numcomps;
  uint32_t bpc = 0;
  CHECK_OR_RETURN(opj_image->numcomps >= 1 && opj_image->numcomps <= 4, quiet);
  for (uint32_t i = 0; i < opj_image->numcomps; ++i) {
    const opj_image_comp_t& comp = opj_image->comps[i];

    CHECK_OR_RETURN(!comp.sgnd, quiet);
    if (comp.alpha) {
      // With OPJ_CODEC_J2K, this is never reached.
      CHECK_OR_RETURN(alpha_channel_index == opj_image->numcomps, quiet);
      alpha_channel_index = i;
    }
    CHECK_OR_RETURN(bpc == 0 || bpc == comp.prec, quiet);
    bpc = comp.prec;
    CHECK_OR_RETURN(bpc == 8 || bpc == 16, quiet);
  }

  const WP2SampleFormat format = alpha_channel_index == opj_image->numcomps
                                     ? (bpc == 8 ? WP2_RGB_24 : WP2_RGB_48)
                                     : (bpc == 8 ? WP2_RGBA_32 : WP2_RGBA_64);
  CHECK_OR_RETURN(WP2FormatNumChannels(format) == opj_image->numcomps, quiet);
  CHECK_OR_RETURN(
      WP2FormatHasAlpha(format) == (alpha_channel_index != opj_image->numcomps),
      quiet);
  uint32_t alpha_channel_index_verif;
  if (WP2FormatHasAlpha(format, &alpha_channel_index_verif)) {
    CHECK_OR_RETURN(alpha_channel_index == alpha_channel_index_verif, quiet);
  }

  Image image;
  image.reserve(1);
  image.emplace_back(WP2::ArgbBuffer(format), /*duration_ms=*/0);
  CHECK_OR_RETURN(
      image.back().pixels.Resize(opj_image->comps[0].w,
                                 opj_image->comps[0].h) == WP2_STATUS_OK,
      quiet);

  WP2::ArgbBuffer& pixels = image.front().pixels;
  for (uint32_t c = 0; c < opj_image->numcomps; ++c) {
    for (uint32_t y = 0; y < pixels.height(); ++y) {
      const OPJ_INT32* const opj_row =
          opj_image->comps[c].data + y * pixels.width();
      if (WP2Formatbpc(pixels.format()) == 8) {
        uint8_t* const row = pixels.GetRow8(y);
        for (uint32_t x = 0; x < pixels.width(); ++x) {
          row[opj_image->numcomps * x + c] = static_cast<uint8_t>(opj_row[x]);
        }
      } else {
        uint16_t* const row = pixels.GetRow16(y);
        for (uint32_t x = 0; x < pixels.width(); ++x) {
          row[opj_image->numcomps * x + c] = static_cast<uint16_t>(opj_row[x]);
        }
      }
    }
  }
  return std::pair<Image, double>(std::move(image), 0);
}

#else
StatusOr<WP2::Data> EncodeOpenjpeg(const TaskInput&, const Image&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_OPENJPEG";
}
StatusOr<std::pair<Image, double>> DecodeOpenjpeg(const TaskInput&,
                                                  const WP2::Data&,
                                                  bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_OPENJPEG";
}
#endif  // HAS_OPENJPEG

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
