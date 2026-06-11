// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Demux API.
// Enables extraction of image and extended format data from WebP files.

// Code Example: Demuxing WebP data to extract all the frames, ICC profile
// and EXIF/XMP metadata.
/*
  ccgen_WebPDemuxer* demux = ccgen_WebPDemux(&webp_data);

  uint32_t width = ccgen_WebPDemuxGetI(demux, CCGEN_WEBP_FF_CANVAS_WIDTH);
  uint32_t height = ccgen_WebPDemuxGetI(demux, CCGEN_WEBP_FF_CANVAS_HEIGHT);
  // ... (Get information about the features present in the WebP file).
  uint32_t flags = ccgen_WebPDemuxGetI(demux, CCGEN_WEBP_FF_FORMAT_FLAGS);

  // ... (Iterate over all frames).
  ccgen_WebPIterator iter;
  if (ccgen_WebPDemuxGetFrame(demux, 1, &iter)) {
    do {
      // ... (Consume 'iter'; e.g. Decode 'iter.fragment' with ccgen_WebPDecode(),
      // ... and get other frame properties like width, height, offsets etc.
      // ... see 'struct ccgen_WebPIterator' below for more info).
    } while (ccgen_WebPDemuxNextFrame(&iter));
    ccgen_WebPDemuxReleaseIterator(&iter);
  }

  // ... (Extract metadata).
  ccgen_WebPChunkIterator chunk_iter;
  if (flags & ICCP_FLAG) ccgen_WebPDemuxGetChunk(demux, "ICCP", 1, &chunk_iter);
  // ... (Consume the ICC profile in 'chunk_iter.chunk').
  ccgen_WebPDemuxReleaseChunkIterator(&chunk_iter);
  if (flags & EXIF_FLAG) ccgen_WebPDemuxGetChunk(demux, "EXIF", 1, &chunk_iter);
  // ... (Consume the EXIF metadata in 'chunk_iter.chunk').
  ccgen_WebPDemuxReleaseChunkIterator(&chunk_iter);
  if (flags & XMP_FLAG) ccgen_WebPDemuxGetChunk(demux, "XMP ", 1, &chunk_iter);
  // ... (Consume the XMP metadata in 'chunk_iter.chunk').
  ccgen_WebPDemuxReleaseChunkIterator(&chunk_iter);
  ccgen_WebPDemuxDelete(demux);
*/

#ifndef CCGEN_WEBP_DEMUX_H_
#define CCGEN_WEBP_DEMUX_H_

#include <stddef.h>

#include "./decode_rs.h"  // for CCGEN_WEBP_CSP_MODE
#include "./mux_types_rs.h"
#include "./types_rs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CCGEN_WEBP_DEMUX_ABI_VERSION 0x0107  // MAJOR(8b) + MINOR(8b)

// Note: forward declaring enumerations is not allowed in (strict) C and C++,
// the types are left here for reference.
// typedef enum ccgen_WebPDemuxState ccgen_WebPDemuxState;
// typedef enum ccgen_WebPFormatFeature ccgen_WebPFormatFeature;
typedef struct ccgen_WebPDemuxer ccgen_WebPDemuxer;
typedef struct ccgen_WebPIterator ccgen_WebPIterator;
typedef struct ccgen_WebPChunkIterator ccgen_WebPChunkIterator;
typedef struct ccgen_WebPAnimInfo ccgen_WebPAnimInfo;
typedef struct ccgen_WebPAnimDecoderOptions ccgen_WebPAnimDecoderOptions;

//------------------------------------------------------------------------------

// Returns the version number of the demux library, packed in hexadecimal using
// 8bits for each of major/minor/revision. E.g: v2.5.7 is 0x020507.
CCGEN_WEBP_EXTERN int ccgen_WebPGetDemuxVersion(void);

//------------------------------------------------------------------------------
// Life of a Demux object

typedef enum ccgen_WebPDemuxState {
  CCGEN_WEBP_DEMUX_PARSE_ERROR = -1,    // An error occurred while parsing.
  CCGEN_WEBP_DEMUX_PARSING_HEADER = 0,  // Not enough data to parse full header.
  CCGEN_WEBP_DEMUX_PARSED_HEADER = 1,   // Header parsing complete,
                                  // data may be available.
  CCGEN_WEBP_DEMUX_DONE = 2             // Entire file has been parsed.
} ccgen_WebPDemuxState;

// Internal, version-checked, entry point
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN ccgen_WebPDemuxer* ccgen_WebPDemuxInternal(const ccgen_WebPData*, int,
                                                          ccgen_WebPDemuxState*, int);

// Parses the full WebP file given by 'data'. For single images the WebP file
// header alone or the file header and the chunk header may be absent.
// Returns a ccgen_WebPDemuxer object on successful parse, NULL otherwise.
CCGEN_WEBP_NODISCARD static CCGEN_WEBP_INLINE ccgen_WebPDemuxer* ccgen_WebPDemux(const ccgen_WebPData* data) {
  return ccgen_WebPDemuxInternal(data, 0, NULL, CCGEN_WEBP_DEMUX_ABI_VERSION);
}

// Parses the possibly incomplete WebP file given by 'data'.
// If 'state' is non-NULL it will be set to indicate the status of the demuxer.
// Returns NULL in case of error or if there isn't enough data to start parsing;
// and a ccgen_WebPDemuxer object on successful parse.
// Note that ccgen_WebPDemuxer keeps internal pointers to 'data' memory segment.
// If this data is volatile, the demuxer object should be deleted (by calling
// ccgen_WebPDemuxDelete()) and ccgen_WebPDemuxPartial() called again on the new data.
// This is usually an inexpensive operation.
CCGEN_WEBP_NODISCARD static CCGEN_WEBP_INLINE ccgen_WebPDemuxer* ccgen_WebPDemuxPartial(
    const ccgen_WebPData* data, ccgen_WebPDemuxState* state) {
  return ccgen_WebPDemuxInternal(data, 1, state, CCGEN_WEBP_DEMUX_ABI_VERSION);
}

// Frees memory associated with 'dmux'.
CCGEN_WEBP_EXTERN void ccgen_WebPDemuxDelete(ccgen_WebPDemuxer* dmux);

//------------------------------------------------------------------------------
// Data/information extraction.

typedef enum ccgen_WebPFormatFeature {
  CCGEN_WEBP_FF_FORMAT_FLAGS,  // bit-wise combination of ccgen_WebPFeatureFlags
                         // corresponding to the 'VP8X' chunk (if present).
  CCGEN_WEBP_FF_CANVAS_WIDTH,
  CCGEN_WEBP_FF_CANVAS_HEIGHT,
  CCGEN_WEBP_FF_LOOP_COUNT,        // only relevant for animated file
  CCGEN_WEBP_FF_BACKGROUND_COLOR,  // idem.
  CCGEN_WEBP_FF_FRAME_COUNT        // Number of frames present in the demux object.
                             // In case of a partial demux, this is the number
                             // of frames seen so far, with the last frame
                             // possibly being partial.
} ccgen_WebPFormatFeature;

// Get the 'feature' value from the 'dmux'.
// NOTE: values are only valid if ccgen_WebPDemux() was used or ccgen_WebPDemuxPartial()
// returned a state > CCGEN_WEBP_DEMUX_PARSING_HEADER.
// If 'feature' is CCGEN_WEBP_FF_FORMAT_FLAGS, the returned value is a bit-wise
// combination of ccgen_WebPFeatureFlags values.
// If 'feature' is CCGEN_WEBP_FF_LOOP_COUNT, CCGEN_WEBP_FF_BACKGROUND_COLOR, the returned
// value is only meaningful if the bitstream is animated.
CCGEN_WEBP_EXTERN uint32_t ccgen_WebPDemuxGetI(const ccgen_WebPDemuxer* dmux,
                                   ccgen_WebPFormatFeature feature);

//------------------------------------------------------------------------------
// Frame iteration.

struct ccgen_WebPIterator {
  int frame_num;
  int num_frames;                     // equivalent to CCGEN_WEBP_FF_FRAME_COUNT.
  int x_offset, y_offset;             // offset relative to the canvas.
  int width, height;                  // dimensions of this frame.
  int duration;                       // display duration in milliseconds.
  ccgen_WebPMuxAnimDispose dispose_method;  // dispose method for the frame.
  int complete;  // true if 'fragment' contains a full frame. partial images
                 // may still be decoded with the WebP incremental decoder.
  ccgen_WebPData fragment;  // The frame given by 'frame_num'. Note for historical
                      // reasons this is called a fragment.
  int has_alpha;      // True if the frame contains transparency.
  ccgen_WebPMuxAnimBlend blend_method;  // Blend operation for the frame.

  uint32_t pad[2];  // padding for later use.
  void* private_;   // for internal use only.
};

// Retrieves frame 'frame_number' from 'dmux'.
// 'iter->fragment' points to the frame on return from this function.
// Setting 'frame_number' equal to 0 will return the last frame of the image.
// Returns false if 'dmux' is NULL or frame 'frame_number' is not present.
// Call ccgen_WebPDemuxReleaseIterator() when use of the iterator is complete.
// NOTE: 'dmux' must persist for the lifetime of 'iter'.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPDemuxGetFrame(const ccgen_WebPDemuxer* dmux,
                                                 int frame_number,
                                                 ccgen_WebPIterator* iter);

// Sets 'iter->fragment' to point to the next ('iter->frame_num' + 1) or
// previous ('iter->frame_num' - 1) frame. These functions do not loop.
// Returns true on success, false otherwise.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPDemuxNextFrame(ccgen_WebPIterator* iter);
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPDemuxPrevFrame(ccgen_WebPIterator* iter);

// Releases any memory associated with 'iter'.
// Must be called before any subsequent calls to ccgen_WebPDemuxGetChunk() on the same
// iter. Also, must be called before destroying the associated ccgen_WebPDemuxer with
// ccgen_WebPDemuxDelete().
CCGEN_WEBP_EXTERN void ccgen_WebPDemuxReleaseIterator(ccgen_WebPIterator* iter);

//------------------------------------------------------------------------------
// Chunk iteration.

struct ccgen_WebPChunkIterator {
  // The current and total number of chunks with the fourcc given to
  // ccgen_WebPDemuxGetChunk().
  int chunk_num;
  int num_chunks;
  ccgen_WebPData chunk;  // The payload of the chunk.

  uint32_t pad[6];  // padding for later use
  void* private_;
};

// Retrieves the 'chunk_number' instance of the chunk with id 'fourcc' from
// 'dmux'.
// 'fourcc' is a character array containing the fourcc of the chunk to return,
// e.g., "ICCP", "XMP ", "EXIF", etc.
// Setting 'chunk_number' equal to 0 will return the last chunk in a set.
// Returns true if the chunk is found, false otherwise. Image related chunk
// payloads are accessed through ccgen_WebPDemuxGetFrame() and related functions.
// Call ccgen_WebPDemuxReleaseChunkIterator() when use of the iterator is complete.
// NOTE: 'dmux' must persist for the lifetime of the iterator.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPDemuxGetChunk(const ccgen_WebPDemuxer* dmux,
                                                 const char fourcc[4],
                                                 int chunk_number,
                                                 ccgen_WebPChunkIterator* iter);

// Sets 'iter->chunk' to point to the next ('iter->chunk_num' + 1) or previous
// ('iter->chunk_num' - 1) chunk. These functions do not loop.
// Returns true on success, false otherwise.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPDemuxNextChunk(ccgen_WebPChunkIterator* iter);
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPDemuxPrevChunk(ccgen_WebPChunkIterator* iter);

// Releases any memory associated with 'iter'.
// Must be called before destroying the associated ccgen_WebPDemuxer with
// ccgen_WebPDemuxDelete().
CCGEN_WEBP_EXTERN void ccgen_WebPDemuxReleaseChunkIterator(ccgen_WebPChunkIterator* iter);

//------------------------------------------------------------------------------
// ccgen_WebPAnimDecoder API
//
// This API allows decoding (possibly) animated WebP images.
//
// Code Example:
/*
  ccgen_WebPAnimDecoderOptions dec_options;
  ccgen_WebPAnimDecoderOptionsInit(&dec_options);
  // Tune 'dec_options' as needed.
  ccgen_WebPAnimDecoder* dec = ccgen_WebPAnimDecoderNew(webp_data, &dec_options);
  ccgen_WebPAnimInfo anim_info;
  ccgen_WebPAnimDecoderGetInfo(dec, &anim_info);
  for (uint32_t i = 0; i < anim_info.loop_count; ++i) {
    while (ccgen_WebPAnimDecoderHasMoreFrames(dec)) {
      uint8_t* buf;
      int timestamp;
      ccgen_WebPAnimDecoderGetNext(dec, &buf, &timestamp);
      // ... (Render 'buf' based on 'timestamp').
      // ... (Do NOT free 'buf', as it is owned by 'dec').
    }
    ccgen_WebPAnimDecoderReset(dec);
  }
  const ccgen_WebPDemuxer* demuxer = ccgen_WebPAnimDecoderGetDemuxer(dec);
  // ... (Do something using 'demuxer'; e.g. get EXIF/XMP/ICC data).
  ccgen_WebPAnimDecoderDelete(dec);
*/

typedef struct ccgen_WebPAnimDecoder ccgen_WebPAnimDecoder;  // Main opaque object.

// Global options.
struct ccgen_WebPAnimDecoderOptions {
  // Output colorspace. Only the following modes are supported:
  // MODE_RGBA, MODE_BGRA, MODE_rgbA and MODE_bgrA.
  CCGEN_WEBP_CSP_MODE color_mode;
  int use_threads;      // If true, use multi-threaded decoding.
  uint32_t padding[7];  // Padding for later use.
};

// Internal, version-checked, entry point.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPAnimDecoderOptionsInitInternal(
    ccgen_WebPAnimDecoderOptions*, int);

// Should always be called, to initialize a fresh ccgen_WebPAnimDecoderOptions
// structure before modification. Returns false in case of version mismatch.
// ccgen_WebPAnimDecoderOptionsInit() must have succeeded before using the
// 'dec_options' object.
CCGEN_WEBP_NODISCARD static CCGEN_WEBP_INLINE int ccgen_WebPAnimDecoderOptionsInit(
    ccgen_WebPAnimDecoderOptions* dec_options) {
  return ccgen_WebPAnimDecoderOptionsInitInternal(dec_options,
                                            CCGEN_WEBP_DEMUX_ABI_VERSION);
}

// Internal, version-checked, entry point.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN ccgen_WebPAnimDecoder* ccgen_WebPAnimDecoderNewInternal(
    const ccgen_WebPData*, const ccgen_WebPAnimDecoderOptions*, int);

// Creates and initializes a ccgen_WebPAnimDecoder object.
// Parameters:
//   webp_data - (in) WebP bitstream. This should remain unchanged during the
//                    lifetime of the output ccgen_WebPAnimDecoder object.
//   dec_options - (in) decoding options. Can be passed NULL to choose
//                      reasonable defaults (in particular, color mode MODE_RGBA
//                      will be picked).
// Returns:
//   A pointer to the newly created ccgen_WebPAnimDecoder object, or NULL in case of
//   parsing error, invalid option or memory error.
CCGEN_WEBP_NODISCARD static CCGEN_WEBP_INLINE ccgen_WebPAnimDecoder* ccgen_WebPAnimDecoderNew(
    const ccgen_WebPData* webp_data, const ccgen_WebPAnimDecoderOptions* dec_options) {
  return ccgen_WebPAnimDecoderNewInternal(webp_data, dec_options,
                                    CCGEN_WEBP_DEMUX_ABI_VERSION);
}

// Global information about the animation..
struct ccgen_WebPAnimInfo {
  uint32_t canvas_width;
  uint32_t canvas_height;
  uint32_t loop_count;
  uint32_t bgcolor;
  uint32_t frame_count;
  uint32_t pad[4];  // padding for later use
};

// Get global information about the animation.
// Parameters:
//   dec - (in) decoder instance to get information from.
//   info - (out) global information fetched from the animation.
// Returns:
//   True on success.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPAnimDecoderGetInfo(
    const ccgen_WebPAnimDecoder* dec, ccgen_WebPAnimInfo* info);

// Fetch the next frame from 'dec' based on options supplied to
// ccgen_WebPAnimDecoderNew(). This will be a fully reconstructed canvas of size
// 'canvas_width * 4 * canvas_height', and not just the frame sub-rectangle. The
// returned buffer 'buf' is valid only until the next call to
// ccgen_WebPAnimDecoderGetNext(), ccgen_WebPAnimDecoderReset() or ccgen_WebPAnimDecoderDelete().
// Parameters:
//   dec - (in/out) decoder instance from which the next frame is to be fetched.
//   buf - (out) decoded frame.
//   timestamp - (out) timestamp of the frame in milliseconds.
// Returns:
//   False if any of the arguments are NULL, or if there is a parsing or
//   decoding error, or if there are no more frames. Otherwise, returns true.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPAnimDecoderGetNext(ccgen_WebPAnimDecoder* dec,
                                                      uint8_t** buf,
                                                      int* timestamp);

// Check if there are more frames left to decode.
// Parameters:
//   dec - (in) decoder instance to be checked.
// Returns:
//   True if 'dec' is not NULL and some frames are yet to be decoded.
//   Otherwise, returns false.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPAnimDecoderHasMoreFrames(
    const ccgen_WebPAnimDecoder* dec);

// Resets the ccgen_WebPAnimDecoder object, so that next call to
// ccgen_WebPAnimDecoderGetNext() will restart decoding from 1st frame. This would be
// helpful when all frames need to be decoded multiple times (e.g.
// info.loop_count times) without destroying and recreating the 'dec' object.
// Parameters:
//   dec - (in/out) decoder instance to be reset
CCGEN_WEBP_EXTERN void ccgen_WebPAnimDecoderReset(ccgen_WebPAnimDecoder* dec);

// Grab the internal demuxer object.
// Getting the demuxer object can be useful if one wants to use operations only
// available through demuxer; e.g. to get XMP/EXIF/ICC metadata. The returned
// demuxer object is owned by 'dec' and is valid only until the next call to
// ccgen_WebPAnimDecoderDelete().
//
// Parameters:
//   dec - (in) decoder instance from which the demuxer object is to be fetched.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN const ccgen_WebPDemuxer* ccgen_WebPAnimDecoderGetDemuxer(
    const ccgen_WebPAnimDecoder* dec);

// Deletes the ccgen_WebPAnimDecoder object.
// Parameters:
//   dec - (in/out) decoder instance to be deleted
CCGEN_WEBP_EXTERN void ccgen_WebPAnimDecoderDelete(ccgen_WebPAnimDecoder* dec);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // CCGEN_WEBP_DEMUX_H_
