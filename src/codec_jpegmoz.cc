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

#include "src/codec_jpegmoz.h"

#if defined(HAS_JPEGMOZ) && defined(HAS_WEBP2)
#include <csetjmp>
#include <cstdint>
#include <cstdlib>
#endif
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

#if defined(HAS_JPEGMOZ)
#include "third_party/mozjpeg/jpeglib.h"
#endif

namespace codec_compare_gen {

std::string JpegmozVersion() {
#if defined(HAS_JPEGMOZ)
  // JPEG_LIB_VERSION in jconfig.h seems to be the version of the turbojpeg
  // library MozJPEG is based on. I could not find an API for MozJPEG version.
  // Hardcode the version tied to the GitHub commit used in deps.sh.
  return "4.1.5";  // 6c9f0897afa1c2738d7222a0a9ab49e8b536a267
#else
  return "n/a";
#endif
}

std::vector<int> JpegmozLossyQualities() {
  std::vector<int> qualities(101);
  std::iota(qualities.begin(), qualities.end(), 0);
  return qualities;
}

#if defined(HAS_WEBP2)

#if defined(HAS_JPEGMOZ)

namespace {

void jpeg_catch_error(j_common_ptr cinfo) {
  // (*cinfo->err->output_message) (cinfo); // to print encountered errors
  jmp_buf* jpeg_jmpbuf = reinterpret_cast<jmp_buf*>(cinfo->client_data);
  jpeg_destroy(cinfo);
  longjmp(*jpeg_jmpbuf, 1);
}

}  // namespace

StatusOr<WP2::Data> EncodeJpegmoz(const TaskInput& input,
                                  const WP2::ArgbBuffer& original_image,
                                  bool quiet) {
  CHECK_OR_RETURN(input.codec_settings.effort == 0, quiet);

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  jmp_buf jpeg_jmpbuf;  // recovery point in case of error
  unsigned char* outbuffer = nullptr;

  cinfo.err = jpeg_std_error(&jerr);
  cinfo.client_data = &jpeg_jmpbuf;
  jerr.error_exit = jpeg_catch_error;
  if (setjmp(jpeg_jmpbuf)) {
    std::free(outbuffer);
    return Status::kUnknownError;
  }

  jpeg_create_compress(&cinfo);

  unsigned long outsize = 0;
  jpeg_mem_dest(&cinfo, &outbuffer, &outsize);

  cinfo.image_width = static_cast<int>(original_image.width());
  cinfo.image_height = static_cast<int>(original_image.height());
  CHECK_OR_RETURN(original_image.format() == WP2_RGB_24, quiet);
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  cinfo.optimize_coding = TRUE;

  cinfo.density_unit = 1;  // JFIF code for pixel size units: 1 = in, 2 = cm
  cinfo.X_density = 300;   // Horizontal pixel density (ppi)
  cinfo.Y_density = 300;   // Vertical pixel density   (ppi)
  jpeg_set_quality(&cinfo, input.codec_settings.quality,
                   /*force_baseline=*/TRUE);

  jpeg_simple_progression(&cinfo);

  const bool chroma_downsampling = false;  // Use 4:4:4.
  if (chroma_downsampling) {
    // cf https://zpl.fi/chroma-subsampling-and-jpeg-sampling-factors/
    cinfo.comp_info[0].h_samp_factor = 2;
    cinfo.comp_info[0].v_samp_factor = 2;
    for (int i = 1; i < cinfo.num_components; ++i) {
      cinfo.comp_info[i].h_samp_factor = 1;
      cinfo.comp_info[i].v_samp_factor = 1;
    }
  } else {
    // Turn off chroma subsampling (it is on by default).  For more details on
    // chroma subsampling, see http://en.wikipedia.org/wiki/Chroma_subsampling.
    for (int i = 0; i < cinfo.num_components; ++i) {
      cinfo.comp_info[i].h_samp_factor = 1;
      cinfo.comp_info[i].v_samp_factor = 1;
    }
  }

  jpeg_start_compress(&cinfo, TRUE);

  int num_scanlines = 0;
  while (cinfo.next_scanline < cinfo.image_height) {
    JSAMPROW row_pointer[1];  // pointer to JSAMPLE rows
    row_pointer[0] = reinterpret_cast<JSAMPLE*>(const_cast<JSAMPLE*>(
        original_image.GetRow8(static_cast<uint32_t>(cinfo.next_scanline))));
    num_scanlines = jpeg_write_scanlines(&cinfo, row_pointer, 1);
    if (num_scanlines != 1) break;
  }
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  if (num_scanlines != 1) {
    std::free(outbuffer);
    CHECK_OR_RETURN(false, quiet) << "num_scanlines: " << num_scanlines;
  }

  WP2::Data data;
  data.bytes = outbuffer;
  data.size = static_cast<size_t>(outsize);
  return data;
}

StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeJpegmoz(
    const TaskInput& input, const WP2::Data& encoded_image, bool quiet) {
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, encoded_image.bytes, encoded_image.size);
  const int result = jpeg_read_header(&cinfo, TRUE);
  if (result != 1) {
    jpeg_destroy_decompress(&cinfo);
    CHECK_OR_RETURN(false, quiet) << "jpeg_read_header() failed: " << result;
  }
  (void)jpeg_start_decompress(&cinfo);

  std::pair<WP2::ArgbBuffer, double> image_and_color_conversion_duration(
      WP2_RGB_24, 0.);
  WP2::ArgbBuffer& image = image_and_color_conversion_duration.first;
  CHECK_OR_RETURN(
      image.Resize(static_cast<uint32_t>(cinfo.output_width),
                   static_cast<uint32_t>(cinfo.output_height)) == WP2_STATUS_OK,
      quiet);
  CHECK_OR_RETURN(
      image.stride() ==
          static_cast<uint32_t>(cinfo.output_width * cinfo.output_components),
      quiet);

  int num_scanlines = 0;
  while (cinfo.output_scanline < cinfo.output_height) {
    JSAMPROW row_pointer[1];  // pointer to JSAMPLE rows
    row_pointer[0] = reinterpret_cast<JSAMPLE*>(
        image.GetRow8(static_cast<uint32_t>(cinfo.output_scanline)));
    num_scanlines = jpeg_read_scanlines(&cinfo, row_pointer, 1);
    if (num_scanlines != 1) break;
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  CHECK_OR_RETURN(num_scanlines == 1, quiet)
      << "num_scanlines: " << num_scanlines;
  return image_and_color_conversion_duration;
}

#else
StatusOr<WP2::Data> EncodeJpegmoz(const TaskInput&, const WP2::ArgbBuffer&,
                                  bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_JPEGMOZ";
}
StatusOr<std::pair<WP2::ArgbBuffer, double>> DecodeJpegmoz(const TaskInput&,
                                                           const WP2::Data&,
                                                           bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_JPEGMOZ";
}
#endif  // HAS_JPEGMOZ

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
