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

#include "src/codec_jpegturbo.h"

#include <cstdint>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/serialization.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

#if defined(HAS_JPEGTURBO)
#include "jconfigint.h"
#include "turbojpeg.h"
#endif

namespace codec_compare_gen {

std::string JpegturboVersion() {
#if defined(HAS_JPEGTURBO)
  return VERSION "." BUILD;
#else
  return "n/a";
#endif
}

std::vector<int> JpegturboLossyQualities() {
  std::vector<int> qualities(101);
  std::iota(qualities.begin(), qualities.end(), 0);
  return qualities;
}

#if defined(HAS_WEBP2)

#if defined(HAS_JPEGTURBO)

constexpr int kPitch = 0;

StatusOr<WP2::Data> EncodeJpegturbo(const TaskInput& input,
                                    const WP2::ArgbBuffer& original_image,
                                    bool quiet) {
  CHECK_OR_RETURN(input.codec_settings.effort == 0, quiet);
  CHECK_OR_RETURN(original_image.format() == WP2_RGB_24, quiet);
  TJSAMP chroma_subsampling;
  if (input.codec_settings.chroma_subsampling == Subsampling::kDefault ||
      input.codec_settings.chroma_subsampling == Subsampling::k420) {
    chroma_subsampling = TJSAMP_420;
  } else {
    CHECK_OR_RETURN(
        input.codec_settings.chroma_subsampling == Subsampling::k444, quiet)
        << "jpegturbo does not support chroma subsampling "
        << SubsamplingToString(input.codec_settings.chroma_subsampling);
    chroma_subsampling = TJSAMP_444;
  }

  long unsigned int compressed_num_bytes = 0;
  unsigned char* compressed_image = nullptr;

  const tjhandle handle = tjInitCompress();
  CHECK_OR_RETURN(handle != nullptr, quiet) << "tjInitCompress() failed";
  int result =
      tjCompress2(handle, original_image.GetRow8(0),
                  static_cast<int>(original_image.width()), kPitch,
                  static_cast<int>(original_image.height()), TJPF_RGB,
                  &compressed_image, &compressed_num_bytes, chroma_subsampling,
                  input.codec_settings.quality, TJFLAG_FASTDCT);
  CHECK_OR_RETURN(result == 0, quiet) << "tjCompress2() failed with " << result;
  result = tjDestroy(handle);
  CHECK_OR_RETURN(result == 0, quiet) << "tjDestroy() failed with " << result;
  // tjFree(compressed_image); // Data is moved instead.
  WP2::Data data;
  data.bytes = compressed_image;
  data.size = compressed_num_bytes;
  return data;
}

StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeJpegturbo(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  int jpegSubsamp, width, height;

  const tjhandle handle = tjInitDecompress();
  CHECK_OR_RETURN(handle != nullptr, quiet) << "tjInitDecompress() failed";

  int result =
      tjDecompressHeader2(handle, encoded_image.bytes,
                          static_cast<unsigned long>(encoded_image.size),
                          &width, &height, &jpegSubsamp);
  CHECK_OR_RETURN(result == 0, quiet)
      << "tjDecompressHeader2() failed with " << result;

  std::pair<WP2::ArgbBuffer, double> image_and_color_conversion_duration(
      WP2_RGB_24, 0.);
  WP2::ArgbBuffer& image = image_and_color_conversion_duration.first;
  CHECK_OR_RETURN(image.Resize(static_cast<uint32_t>(width),
                               static_cast<uint32_t>(height)) == WP2_STATUS_OK,
                  quiet);

  result = tjDecompress2(handle, encoded_image.bytes,
                         static_cast<unsigned long>(encoded_image.size),
                         image.GetRow8(0), width, kPitch, height, TJPF_RGB,
                         TJFLAG_FASTDCT);
  CHECK_OR_RETURN(result == 0, quiet)
      << "tjDecompress2() failed with " << result;

  result = tjDestroy(handle);
  CHECK_OR_RETURN(result == 0, quiet)
      << "tjDestroy() (dec) failed with " << result;
  return image_and_color_conversion_duration;
}

#else
StatusOr<WP2::Data> EncodeJpegturbo(const TaskInput&, const WP2::ArgbBuffer&,
                                    bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_JPEGTURBO";
}
StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeJpegturbo(const TaskInput&,
                                                             const WP2::Data&,
                                                             bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_JPEGTURBO";
}
#endif  // HAS_JPEGTURBO

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
