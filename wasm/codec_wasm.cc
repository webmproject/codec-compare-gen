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

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/codec.h"
#include "src/framework.h"

namespace codec_compare_gen {
struct EncodedBytesAndDecodeStats {
  emscripten::val encoded_bytes;
  std::string encoded_bytes_mime_type;

  // Same fields as TaskOutput.
  uint32_t width;
  uint32_t height;
  uint32_t bit_depth;
  uint32_t num_frames;
  size_t encoded_size;
  double encoding_duration;
  double decoding_duration;
  double decoding_color_conversion_duration;
  float psnr;
  float ssim;

  std::string description;
};

EncodedBytesAndDecodeStats WasmGetEncodedBytesAndDecodeStats(
    const emscripten::val& bytes_val, Codec codec, Subsampling subsampling,
    int effort, int quality) {
  const std::vector<uint8_t> bytes =
      emscripten::convertJSArrayToNumberVector<uint8_t>(bytes_val);
  auto status_or = GetEncodedBytesAndDecodeStats(
      bytes.data(), bytes.size(), codec, subsampling, effort, quality,
      /*quiet=*/false);
  if (status_or.status != Status::kOk) {
    emscripten::val::global("Error")(std::string("Failed to decode image"))
        .throw_();
  }
  auto& [encoded_bytes, encoded_bytes_mime_type, task] = status_or.value;
  const CodecSettings& codec_settings = task.task_input.codec_settings;
  const CodecMetadata codec_metadata = GetCodecMetadata(codec_settings.codec);

  const float psnr =
      task.distortions.empty() ? kNoDistortion : task.distortions[0];
  const float ssim =
      task.distortions.size() < 2 ? kNoDistortion : task.distortions[1];
  const bool lossless = codec_settings.quality == kQualityLossless;
  std::string description = codec_metadata.pretty_name(
      lossless, codec_settings.chroma_subsampling, codec_settings.effort);
  if (!lossless) {
    description += " q";
    description += std::to_string(codec_settings.quality);
  }

  emscripten::val js_encoded_bytes =
      emscripten::val::global("Uint8Array").new_(encoded_bytes.size());
  js_encoded_bytes.call<void>(
      "set", emscripten::val(emscripten::typed_memory_view(
                 encoded_bytes.size(), encoded_bytes.data())));

  return {std::move(js_encoded_bytes),
          encoded_bytes_mime_type,
          task.image_width,
          task.image_height,
          task.bit_depth,
          task.num_frames,
          task.encoded_size,
          task.encoding_duration,
          task.decoding_duration,
          task.decoding_color_conversion_duration,
          psnr,
          ssim,
          description};
}

}  // namespace codec_compare_gen

EMSCRIPTEN_BINDINGS(codec_compare_gen) {
  using namespace codec_compare_gen;

  emscripten::enum_<Codec>("Codec")
      .value("Avif", Codec::kAvif)
      .value("Webp", Codec::kWebp)
      .value("Webp2", Codec::kWebp2)
      .value("Jpegturbo", Codec::kJpegturbo)
      .value("Jpegsimple", Codec::kJpegsimple)
      .value("Jpegmoz", Codec::kJpegmoz);

  emscripten::enum_<Subsampling>("Subsampling")
      .value("Default", Subsampling::kDefault)
      .value("Yuv444", Subsampling::k444)
      .value("Yuv420", Subsampling::k420);

  emscripten::value_object<EncodedBytesAndDecodeStats>(
      "EncodedBytesAndDecodeStats")
      .field("encoded_bytes", &EncodedBytesAndDecodeStats::encoded_bytes)
      .field("encoded_bytes_mime_type",
             &EncodedBytesAndDecodeStats::encoded_bytes_mime_type)
      .field("width", &EncodedBytesAndDecodeStats::width)
      .field("height", &EncodedBytesAndDecodeStats::height)
      .field("bit_depth", &EncodedBytesAndDecodeStats::bit_depth)
      .field("num_frames", &EncodedBytesAndDecodeStats::num_frames)
      .field("encoded_size", &EncodedBytesAndDecodeStats::encoded_size)
      .field("encoding_duration",
             &EncodedBytesAndDecodeStats::encoding_duration)
      .field("decoding_duration",
             &EncodedBytesAndDecodeStats::decoding_duration)
      .field("decoding_color_conversion_duration",
             &EncodedBytesAndDecodeStats::decoding_color_conversion_duration)
      .field("psnr", &EncodedBytesAndDecodeStats::psnr)
      .field("ssim", &EncodedBytesAndDecodeStats::ssim)
      .field("description", &EncodedBytesAndDecodeStats::description);

  emscripten::function("getEncodedBytesAndDecodeStats",
                       &WasmGetEncodedBytesAndDecodeStats);
}
