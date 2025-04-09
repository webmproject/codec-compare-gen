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

#include "src/codec_ffv1.h"

#if defined(HAS_FFV1) && defined(HAS_WEBP2)
#include <cstddef>
#include <cstdint>
#include <cstring>
#endif
#include <string>
#include <utility>

#include "src/base.h"
#include "src/frame.h"
#include "src/task.h"

#if defined(HAS_WEBP2)
#include "third_party/libwebp2/src/wp2/base.h"
#endif

#if defined(HAS_FFV1)
extern "C" {
#include "third_party/FFmpeg/libavcodec/avcodec.h"
}
#endif

namespace codec_compare_gen {

std::string Ffv1Version() {
#if defined(HAS_FFV1)
  return std::to_string(avcodec_version() >> 16) + "." +
         std::to_string((avcodec_version() >> 8) & 0xff) + "." +
         std::to_string((avcodec_version() >> 0) & 0xff);
#else
  return "n/a";
#endif
}

#if defined(HAS_WEBP2)

#if defined(HAS_FFV1)

namespace {

struct Ffv1 {
 public:
  Ffv1(bool is_encoding)
      : is_encoding(is_encoding),
        codec(is_encoding ? avcodec_find_encoder(AV_CODEC_ID_FFV1)
                          : avcodec_find_decoder(AV_CODEC_ID_FFV1)),
        context(avcodec_alloc_context3(codec)),
        packet(av_packet_alloc()),
        frame(av_frame_alloc()) {}
  ~Ffv1() {
    if (is_encoding) {
      // libavcodec handles the memory for extradata and data.
    } else {
      if (context != nullptr && context->extradata != nullptr) {
        // libavcodec did not allocate the first buffer pointed to by
        // context->extradata, but may have freed it and replaced it with
        // another buffer, which is still to be freed by the user.
        av_free(context->extradata);
        context->extradata = nullptr;
        context->extradata_size = 0;
      }
      if (packet != nullptr && packet->data != nullptr) {
        av_free(packet->data);
        packet->data = nullptr;
        packet->size = 0;
      }
    }

    if (packet != nullptr) av_packet_unref(packet);
    if (frame != nullptr) av_frame_unref(frame);
    if (packet != nullptr) av_packet_free(&packet);
    if (frame != nullptr) av_frame_free(&frame);
    if (context != nullptr) avcodec_free_context(&context);
  }
  const bool is_encoding;
  const AVCodec* const codec = nullptr;
  AVCodecContext* context = nullptr;
  AVPacket* packet = nullptr;
  AVFrame* frame = nullptr;
};

struct Ffv1Container {
  uint32_t width;
  uint32_t height;
  AVPixelFormat format;
  uint32_t extradata_size;
};

}  // namespace

StatusOr<WP2::Data> EncodeFfv1(const TaskInput& input,
                               const Image& original_image, bool quiet) {
  // TODO(yguyon): Support sequences.
  CHECK_OR_RETURN(original_image.size() == 1, quiet);
  const WP2::ArgbBuffer& pixels = original_image.front().pixels;
  // Ffv1 has no effort parameter and is lossless only.
  CHECK_OR_RETURN(input.codec_settings.effort == 0, quiet);
  CHECK_OR_RETURN(input.codec_settings.quality == kQualityLossless, quiet);
  CHECK_OR_RETURN(
      input.codec_settings.chroma_subsampling == Subsampling::k444 ||
          input.codec_settings.chroma_subsampling == Subsampling::kDefault,
      quiet);

  Ffv1 ffv1(/*is_encoding=*/true);
  CHECK_OR_RETURN(ffv1.codec != nullptr, quiet);
  CHECK_OR_RETURN(ffv1.context != nullptr, quiet);
  CHECK_OR_RETURN(ffv1.packet != nullptr, quiet);
  CHECK_OR_RETURN(ffv1.frame != nullptr, quiet);
  ffv1.context->width = static_cast<int>(pixels.width());
  ffv1.context->height = static_cast<int>(pixels.height());
  ffv1.context->time_base = {1, 25};
  ffv1.context->framerate = {25, 1};
  ffv1.context->thread_count = 1;
  // TODO(yguyon): Support 16-bit.
  CHECK_OR_RETURN(WP2Formatbpc(pixels.format()) == 8, quiet);
  ffv1.context->pix_fmt =
      pixels.HasTransparency() ? AV_PIX_FMT_RGB32 : AV_PIX_FMT_0RGB32;

  AVDictionary* opts = NULL;
  if (WP2Formatbpc(pixels.format()) > 8) {
    CHECK_OR_RETURN(av_dict_set(&opts, "coder", "range_tab", 0) == 0, quiet);
  }

  CHECK_OR_RETURN(avcodec_open2(ffv1.context, ffv1.codec, &opts) == 0, quiet);

  av_packet_unref(ffv1.packet);
  av_frame_unref(ffv1.frame);

  ffv1.frame->format = ffv1.context->pix_fmt;
  ffv1.frame->width = ffv1.context->width;
  ffv1.frame->height = ffv1.context->height;
  ffv1.frame->pts = 0;

  CHECK_OR_RETURN(av_frame_get_buffer(ffv1.frame, 0) == 0, quiet);
  CHECK_OR_RETURN(av_frame_make_writable(ffv1.frame) == 0, quiet);

  {
    WP2::ArgbBuffer ffv1_frame_view(
        ffv1.context->pix_fmt == AV_PIX_FMT_0RGB32 ? WP2_BGRX_32 : WP2_BGRA_32);
    CHECK_OR_RETURN(ffv1_frame_view.SetExternal(
                        pixels.width(), pixels.height(), ffv1.frame->data[0],
                        ffv1.frame->linesize[0]) == WP2_STATUS_OK,
                    quiet);
    CHECK_OR_RETURN(ffv1_frame_view.ConvertFrom(pixels) == WP2_STATUS_OK,
                    quiet);
  }

  CHECK_OR_RETURN(avcodec_send_frame(ffv1.context, ffv1.frame) == 0, quiet);
  CHECK_OR_RETURN(avcodec_receive_packet(ffv1.context, ffv1.packet) == 0,
                  quiet);

  // FFV1 is only a codec. Prepend the encoded frame with some custom minimal
  // container.
  const Ffv1Container header{
      pixels.width(), pixels.height(), ffv1.context->pix_fmt,
      static_cast<uint32_t>(ffv1.context->extradata_size)};
  WP2::Data encoded_image;
  CHECK_OR_RETURN(
      encoded_image.CopyFrom(reinterpret_cast<const uint8_t*>(&header),
                             sizeof(header)) == WP2_STATUS_OK,
      quiet);
  CHECK_OR_RETURN(encoded_image.Append(ffv1.context->extradata,
                                       header.extradata_size) == WP2_STATUS_OK,
                  quiet);
  CHECK_OR_RETURN(encoded_image.Append(ffv1.packet->data, ffv1.packet->size) ==
                      WP2_STATUS_OK,
                  quiet);
  return encoded_image;
}

StatusOr<std::pair<Image, double>> DecodeFfv1(const TaskInput& input,
                                              const WP2::Data& encoded_image,
                                              bool quiet) {
  CHECK_OR_RETURN(sizeof(Ffv1Container) < encoded_image.size, quiet);
  const Ffv1Container header{
      *reinterpret_cast<const Ffv1Container*>(encoded_image.bytes)};
  CHECK_OR_RETURN(
      sizeof(Ffv1Container) + header.extradata_size < encoded_image.size,
      quiet);
  const uint8_t* extradata = encoded_image.bytes + sizeof(Ffv1Container);
  const size_t encoded_frame_size =
      encoded_image.size - sizeof(Ffv1Container) - header.extradata_size;
  const uint8_t* encoded_frame =
      encoded_image.bytes + sizeof(Ffv1Container) + header.extradata_size;

  Ffv1 ffv1(/*is_encoding=*/false);
  CHECK_OR_RETURN(ffv1.codec != nullptr, quiet);
  CHECK_OR_RETURN(ffv1.context != nullptr, quiet);
  CHECK_OR_RETURN(ffv1.packet != nullptr, quiet);
  CHECK_OR_RETURN(ffv1.frame != nullptr, quiet);

  ffv1.context->width = header.width;
  ffv1.context->height = header.height;
  ffv1.context->pix_fmt = header.format;
  ffv1.context->time_base = {1, 25};
  ffv1.context->framerate = {25, 1};
  ffv1.context->thread_count = 1;

  // From AVCodecContext::extradata documentation:
  //   Must be allocated with the av_malloc() family of functions.
  //   - decoding: Set/allocated/freed by user.
  // This is freed by Ffv1 destructor and maybe replaced by libavcodec.
  ffv1.context->extradata =
      reinterpret_cast<uint8_t*>(av_malloc(header.extradata_size));
  CHECK_OR_RETURN(ffv1.context->extradata != nullptr, quiet);
  std::memcpy(ffv1.context->extradata, extradata, header.extradata_size);
  ffv1.context->extradata_size = static_cast<int>(header.extradata_size);

  CHECK_OR_RETURN(avcodec_open2(ffv1.context, ffv1.codec, NULL) == 0, quiet);

  av_frame_unref(ffv1.frame);

  // Undocumented but the same as for AVCodecContext::extradata is assumed for
  // AVPacket::data.
  ffv1.packet->data = reinterpret_cast<uint8_t*>(av_malloc(encoded_frame_size));
  CHECK_OR_RETURN(ffv1.packet->data != nullptr, quiet);
  std::memcpy(ffv1.packet->data, encoded_frame, encoded_frame_size);
  ffv1.packet->size = static_cast<int>(encoded_frame_size);

  CHECK_OR_RETURN(avcodec_send_packet(ffv1.context, ffv1.packet) == 0, quiet);
  CHECK_OR_RETURN(avcodec_receive_frame(ffv1.context, ffv1.frame) == 0, quiet);

  WP2::ArgbBuffer ffv1_frame_view(
      ffv1.context->pix_fmt == AV_PIX_FMT_0RGB32 ? WP2_BGRX_32 : WP2_BGRA_32);
  CHECK_OR_RETURN(
      ffv1_frame_view.SetExternal(ffv1.frame->width, ffv1.frame->height,
                                  ffv1.frame->data[0],
                                  ffv1.frame->linesize[0]) == WP2_STATUS_OK,
      quiet);

  Image image;
  image.reserve(1);
  image.emplace_back(WP2::ArgbBuffer(WP2_BGRA_32), /*duration_ms=*/0);
  CHECK_OR_RETURN(
      image.back().pixels.ConvertFrom(ffv1_frame_view) == WP2_STATUS_OK, quiet);

  return std::pair<Image, double>(std::move(image), 0);
}

#else
StatusOr<WP2::Data> EncodeFfv1(const TaskInput&, const Image&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Encoding images requires HAS_FFV1";
}
StatusOr<std::pair<Image, double>> DecodeFfv1(const TaskInput&,
                                              const WP2::Data&, bool quiet) {
  CHECK_OR_RETURN(false, quiet) << "Decoding images requires HAS_FFV1";
}
#endif  // HAS_FFV1

#endif  // HAS_WEBP2

}  // namespace codec_compare_gen
