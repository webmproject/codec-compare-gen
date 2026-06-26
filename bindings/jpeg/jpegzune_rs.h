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

#ifndef THIRD_PARTY_CODEC_COMPARE_GEN_BINDINGS_JPEG_JPEGZUNE_RS_H_
#define THIRD_PARTY_CODEC_COMPARE_GEN_BINDINGS_JPEG_JPEGZUNE_RS_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ccgen_imagejpeg_encoder_version(void);
int ccgen_zunejpeg_version(void);

// Encodes an RGB_24 image to JPEG using the Rust image crate.
// Returns 1 on success, 0 on error.
// On success, *output_bytes points to a buffer allocated by Rust, of size
// *output_size. The caller must free it using ccgen_imagejpeg_free_buffer.
int ccgen_imagejpeg_encode444(const uint8_t* rgb_pixels, size_t width,
                              size_t height, size_t stride, int quality,
                              uint8_t** output_bytes, size_t* output_size);

// Decodes a JPEG image to RGB_24 using the Rust zune-jpeg crate.
// Returns 1 on success, 0 on error.
// On success, *width and *height are set, and *output_bytes points to a buffer
// allocated by Rust, of size (*width) * (*height) * 3. The caller must free it
// using ccgen_imagejpeg_free_buffer.
int ccgen_zunejpeg_decode(const uint8_t* encoded_bytes, size_t encoded_size,
                          size_t* width, size_t* height,
                          uint8_t** output_bytes);

// Frees a buffer allocated by ccgen_imagejpeg_encode444 or
// ccgen_zunejpeg_decode.
void ccgen_imagejpeg_free_buffer(uint8_t* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif  // THIRD_PARTY_CODEC_COMPARE_GEN_BINDINGS_JPEG_JPEGZUNE_RS_H_
