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

#include "src/codec_jpegli.h"

#include <cassert>
#include <csetjmp>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/codec_jpegturbo.h"
#include "src/codec_jpegxl.h"
#include "src/frame.h"
#include "src/serialization.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

#if defined(HAS_JPEGTURBO)
#include "third_party/libjpeg_turbo/src/jpeglib.h"
#endif

#if defined(HAS_JPEGXL)
#include "third_party/libjxl/lib/jpegli/common.h"
#include "third_party/libjxl/lib/jpegli/encode.h"
#endif

namespace codec_compare_gen {

std::string JpegliVersion() {
  return JpegXLVersion() + "_" + JpegturboVersion();
}

std::vector<int> JpegliLossyQualities() {
  std::vector<int> qualities(101);
  std::iota(qualities.begin(), qualities.end(), 0);
  return qualities;
}

#if defined(HAS_WEBP2)

#if defined(HAS_JPEGXL) && defined(HAS_JPEGTURBO)

namespace {

void jpeg_catch_error(j_common_ptr cinfo) {
  // (*cinfo->err->output_message) (cinfo); // to print encountered errors
  jmp_buf* jpeg_jmpbuf = reinterpret_cast<jmp_buf*>(cinfo->client_data);
  jpegli_destroy(cinfo);
  longjmp(*jpeg_jmpbuf, 1);
}

}  // namespace

StatusOr<WP2::Data> EncodeJpegli(const TaskInput& input,
                                 const Image& original_image, bool quiet) {
  CHECK_OR_RETURN(original_image.size() == 1, quiet);
  const WP2::ArgbBuffer& pixels = original_image.front().pixels;
  CHECK_OR_RETURN(input.codec_settings.effort == 0, quiet);

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  jmp_buf jpeg_jmpbuf;  // recovery point in case of error
  unsigned char* outbuffer = nullptr;

  cinfo.err = jpegli_std_error(&jerr);
  cinfo.client_data = &jpeg_jmpbuf;
  jerr.error_exit = jpeg_catch_error;
  if (setjmp(jpeg_jmpbuf)) {
    std::free(outbuffer);
    return Status::kUnknownError;
  }

  jpegli_create_compress(&cinfo);

  unsigned long outsize = 0;
  jpegli_mem_dest(&cinfo, &outbuffer, &outsize);

  cinfo.image_width = static_cast<int>(pixels.width());
  cinfo.image_height = static_cast<int>(pixels.height());
  CHECK_OR_RETURN(pixels.format() == WP2_RGB_24, quiet);
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpegli_set_defaults(&cinfo);
  cinfo.optimize_coding = TRUE;

  cinfo.density_unit = 1;  // JFIF code for pixel size units: 1 = in, 2 = cm
  cinfo.X_density = 300;   // Horizontal pixel density (ppi)
  cinfo.Y_density = 300;   // Vertical pixel density   (ppi)
  jpegli_set_quality(&cinfo, input.codec_settings.quality,
                     /*force_baseline=*/TRUE);

  jpegli_simple_progression(&cinfo);

  if (input.codec_settings.chroma_subsampling == Subsampling::kDefault ||
      input.codec_settings.chroma_subsampling == Subsampling::k420) {
    // cf https://zpl.fi/chroma-subsampling-and-jpeg-sampling-factors/
    cinfo.comp_info[0].h_samp_factor = 2;
    cinfo.comp_info[0].v_samp_factor = 2;
    for (int i = 1; i < cinfo.num_components; ++i) {
      cinfo.comp_info[i].h_samp_factor = 1;
      cinfo.comp_info[i].v_samp_factor = 1;
    }
  } else {
    CHECK_OR_RETURN(
        input.codec_settings.chroma_subsampling == Subsampling::k444, quiet)
        << "jpegli does not support chroma subsampling "
        << SubsamplingToString(input.codec_settings.chroma_subsampling);
    // Turn off chroma subsampling (it is on by default). For more details on
    // chroma subsampling, see http://en.wikipedia.org/wiki/Chroma_subsampling.
    for (int i = 0; i < cinfo.num_components; ++i) {
      cinfo.comp_info[i].h_samp_factor = 1;
      cinfo.comp_info[i].v_samp_factor = 1;
    }
  }

  jpegli_start_compress(&cinfo, TRUE);

  int num_scanlines = 0;
  while (cinfo.next_scanline < cinfo.image_height) {
    JSAMPROW row_pointer[1];  // pointer to JSAMPLE rows
    row_pointer[0] = reinterpret_cast<JSAMPLE*>(const_cast<JSAMPLE*>(
        pixels.GetRow8(static_cast<uint32_t>(cinfo.next_scanline))));
    num_scanlines = jpegli_write_scanlines(&cinfo, row_pointer, 1);
    if (num_scanlines != 1) break;
  }
  jpegli_finish_compress(&cinfo);
  jpegli_destroy_compress(&cinfo);

  if (num_scanlines != 1) {
    std::free(outbuffer);
    CHECK_OR_RETURN(false, quiet) << "num_scanlines: " << num_scanlines;
  }

  WP2::Data data;
  data.bytes = outbuffer;
  data.size = static_cast<size_t>(outsize);
  return data;
}

StatusOr<std::pair<Image, double>> DecodeJpegli(const TaskInput& input,
                                                const WP2::Data& encoded_image,
                                                bool quiet) {
  return DecodeJpegturbo(input, encoded_image, quiet);
}

#else
StatusOr<WP2::Data> EncodeJpegli(const TaskInput&, const Image&, bool quiet) {
  CHECK_OR_RETURN(false, quiet)
      << "Encoding images requires HAS_JPEGXL and HAS_JPEGTURBO";
}
StatusOr<std::pair<Image, double>> DecodeJpegli(const TaskInput&,
                                                const WP2::Data&, bool quiet) {
  CHECK_OR_RETURN(false, quiet)
      << "Decoding images requires HAS_JPEGXL and HAS_JPEGTURBO";
}
#endif  // HAS_JPEGXL && HAS_JPEGTURBO

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
