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

#include <cstdint>
#include <string>
#include <vector>

#include "src/base.h"
#include "src/codec.h"

namespace codec_compare_gen {
struct DecodedImage {
  std::vector<uint8_t> argb;
  uint32_t width;
  uint32_t height;
};

DecodedImage WasmDecodeToArgb(const std::vector<uint8_t>& encoded_image) {
  uint32_t width, height;
  auto status_or =
      DecodeToArgb(encoded_image.data(), encoded_image.size(), &width, &height,
                   /*quiet=*/false);
  if (status_or.status != Status::kOk) {
    emscripten::val::global("Error")(std::string("Failed to decode image"))
        .throw_();
  }
  return {std::move(status_or.value), width, height};
}

std::vector<uint8_t> WasmEncode(const std::vector<uint8_t>& argb,
                                uint32_t width, uint32_t height, Codec codec,
                                Subsampling subsampling, int effort,
                                int quality) {
  auto status_or =
      Encode(argb.data(), width, height, codec, subsampling, effort, quality,
             /*quiet=*/false);
  if (status_or.status != Status::kOk) {
    emscripten::val::global("Error")(std::string("Failed to encode image"))
        .throw_();
  }
  return std::move(status_or.value);
}

}  // namespace codec_compare_gen

EMSCRIPTEN_BINDINGS(codec_compare_gen) {
  using namespace codec_compare_gen;

  emscripten::enum_<Codec>("Codec")
      .value("Webp", Codec::kWebp)
      .value("Webp2", Codec::kWebp2)
      .value("JpegXl", Codec::kJpegXl)
      .value("Avif", Codec::kAvif)
      .value("Jpegturbo", Codec::kJpegturbo)
      .value("Jpegli", Codec::kJpegli)
      .value("Jpegsimple", Codec::kJpegsimple)
      .value("Basis", Codec::kBasis);

  emscripten::enum_<Subsampling>("Subsampling")
      .value("Default", Subsampling::kDefault)
      .value("Yuv444", Subsampling::k444)
      .value("Yuv420", Subsampling::k420);

  emscripten::register_vector<uint8_t>("ByteVector");
  emscripten::value_object<DecodedImage>("DecodedImage")
      .field("argb", &DecodedImage::argb)
      .field("width", &DecodedImage::width)
      .field("height", &DecodedImage::height);

  emscripten::function("decodeToArgb", &WasmDecodeToArgb);
  emscripten::function("encode", &WasmEncode);
}
