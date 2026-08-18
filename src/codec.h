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

#ifndef SRC_CODEC_H_
#define SRC_CODEC_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/frame.h"
#include "src/task.h"
#include "src/wp2/base.h"

namespace codec_compare_gen {

struct CodecMetadata {
  const char* name;

  std::string (*pretty_name)(bool lossless, Subsampling subsampling,
                             int effort);
  std::string (*version)();
  const char* build_options;  // CMake "-D..." flags used to build this codec.
  std::vector<int> (*efforts)();
  std::vector<int> (*lossy_qualities)();
  const char* extension;
  const char* mime_type;
  bool is_supported_by_browsers;
  bool supports_16bit;
  WP2SampleFormat opaque_format;
  WP2SampleFormat transparent_format;  // WP2_FORMAT_NUM if no alpha support.

  // Returns the encoded image bytes.
  StatusOr<WP2::Data> (*encode)(const TaskInput&, const Image&, bool quiet);
  // Returns the decoded image pixels and the color conversion duration if any.
  StatusOr<std::pair<Image, double>> (*decode)(const TaskInput&,
                                               const WP2::Data&, bool quiet);
};

CodecMetadata GetCodecMetadata(Codec codec);
StatusOr<Codec> CodecFromName(const std::string& name, bool quiet);

std::string SubsamplingToPrettyString(bool lossless, Subsampling subsampling);

enum class EncodeMode { kEncode, kEncodeAndSaveToDisk, kLoadFromDisk };

StatusOr<TaskOutput> EncodeDecode(const TaskInput& input,
                                  const std::string& metric_binary_folder_path,
                                  size_t thread_id, EncodeMode encode_mode,
                                  bool quiet);

// Returns the encoded image bytes, the associated mime type, and the decode
// stats. For codecs supported by browsers, the encoded image is the output of
// the codec encoder. For other codecs, it is the bytes of a still PNG or a
// lossless animated WebP that can be displayed by browsers.
StatusOr<std::tuple<std::vector<uint8_t>, const char*, TaskOutput>>
GetEncodedBytesAndDecodeStats(const uint8_t* data, size_t data_size,
                              Codec codec, Subsampling chroma_subsampling,
                              int effort, int quality, bool quiet);

// Decodes the encoded still 8-bit image as ARGB pixel values.
StatusOr<std::vector<uint8_t>> DecodeToArgb(const uint8_t* encoded_image,
                                            size_t encoded_size,
                                            uint32_t* width, uint32_t* height,
                                            bool quiet);

}  // namespace codec_compare_gen

#endif  // SRC_CODEC_H_
