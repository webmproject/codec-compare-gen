// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  RIFF container manipulation and encoding for WebP images.
//
// Authors: Urvang (urvang@google.com)
//          Vikas (vikasa@google.com)

#ifndef CCGEN_WEBP_MUX_H_
#define CCGEN_WEBP_MUX_H_

#include "./mux_types_rs.h"
#include "./types_rs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CCGEN_WEBP_MUX_ABI_VERSION 0x0109  // MAJOR(8b) + MINOR(8b)

//------------------------------------------------------------------------------
// Mux API
//
// This API allows manipulation of WebP container images containing features
// like color profile, metadata, animation.
//
// Code Example#1: Create a ccgen_WebPMux object with image data, color profile and
// XMP metadata.
/*
  int copy_data = 0;
  ccgen_WebPMux* mux = ccgen_WebPMuxNew();
  // ... (Prepare image data).
  ccgen_WebPMuxSetImage(mux, &image, copy_data);
  // ... (Prepare ICCP color profile data).
  ccgen_WebPMuxSetChunk(mux, "ICCP", &icc_profile, copy_data);
  // ... (Prepare XMP metadata).
  ccgen_WebPMuxSetChunk(mux, "XMP ", &xmp, copy_data);
  // Get data from mux in WebP RIFF format.
  ccgen_WebPMuxAssemble(mux, &output_data);
  ccgen_WebPMuxDelete(mux);
  // ... (Consume output_data; e.g. write output_data.bytes to file).
  ccgen_WebPDataClear(&output_data);
*/

// Code Example#2: Get image and color profile data from a WebP file.
/*
  int copy_data = 0;
  // ... (Read data from file).
  ccgen_WebPMux* mux = ccgen_WebPMuxCreate(&data, copy_data);
  ccgen_WebPMuxGetFrame(mux, 1, &image);
  // ... (Consume image; e.g. call ccgen_WebPDecode() to decode the data).
  ccgen_WebPMuxGetChunk(mux, "ICCP", &icc_profile);
  // ... (Consume icc_data).
  ccgen_WebPMuxDelete(mux);
  ccgen_WebPFree(data);
*/

// Note: forward declaring enumerations is not allowed in (strict) C and C++,
// the types are left here for reference.
// typedef enum ccgen_WebPMuxError ccgen_WebPMuxError;
// typedef enum ccgen_WebPChunkId ccgen_WebPChunkId;
typedef struct ccgen_WebPMux ccgen_WebPMux;  // main opaque object.
typedef struct ccgen_WebPMuxFrameInfo ccgen_WebPMuxFrameInfo;
typedef struct ccgen_WebPMuxAnimParams ccgen_WebPMuxAnimParams;
typedef struct ccgen_WebPAnimEncoderOptions ccgen_WebPAnimEncoderOptions;

// Error codes
typedef enum CCGEN_WEBP_NODISCARD ccgen_WebPMuxError {
  CCGEN_WEBP_MUX_OK = 1,
  CCGEN_WEBP_MUX_NOT_FOUND = 0,
  CCGEN_WEBP_MUX_INVALID_ARGUMENT = -1,
  CCGEN_WEBP_MUX_BAD_DATA = -2,
  CCGEN_WEBP_MUX_MEMORY_ERROR = -3,
  CCGEN_WEBP_MUX_NOT_ENOUGH_DATA = -4
} ccgen_WebPMuxError;

// IDs for different types of chunks.
typedef enum ccgen_WebPChunkId {
  CCGEN_WEBP_CHUNK_VP8X,        // VP8X
  CCGEN_WEBP_CHUNK_ICCP,        // ICCP
  CCGEN_WEBP_CHUNK_ANIM,        // ANIM
  CCGEN_WEBP_CHUNK_ANMF,        // ANMF
  CCGEN_WEBP_CHUNK_DEPRECATED,  // (deprecated from FRGM)
  CCGEN_WEBP_CHUNK_ALPHA,       // ALPH
  CCGEN_WEBP_CHUNK_IMAGE,       // VP8/VP8L
  CCGEN_WEBP_CHUNK_EXIF,        // EXIF
  CCGEN_WEBP_CHUNK_XMP,         // XMP
  CCGEN_WEBP_CHUNK_UNKNOWN,     // Other chunks.
  CCGEN_WEBP_CHUNK_NIL
} ccgen_WebPChunkId;

//------------------------------------------------------------------------------

// Returns the version number of the mux library, packed in hexadecimal using
// 8bits for each of major/minor/revision. E.g: v2.5.7 is 0x020507.
CCGEN_WEBP_EXTERN int ccgen_WebPGetMuxVersion(void);

//------------------------------------------------------------------------------
// Life of a Mux object

// Internal, version-checked, entry point
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN ccgen_WebPMux* ccgen_WebPNewInternal(int);

// Creates an empty mux object.
// Returns:
//   A pointer to the newly created empty mux object.
//   Or NULL in case of memory error.
CCGEN_WEBP_NODISCARD static CCGEN_WEBP_INLINE ccgen_WebPMux* ccgen_WebPMuxNew(void) {
  return ccgen_WebPNewInternal(CCGEN_WEBP_MUX_ABI_VERSION);
}

// Deletes the mux object.
// Parameters:
//   mux - (in/out) object to be deleted
CCGEN_WEBP_EXTERN void ccgen_WebPMuxDelete( ccgen_WebPMux* mux);

//------------------------------------------------------------------------------
// Mux creation.

// Internal, version-checked, entry point
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN ccgen_WebPMux* ccgen_WebPMuxCreateInternal(const ccgen_WebPData*, int,
                                                          int);

// Creates a mux object from raw data given in WebP RIFF format.
// Parameters:
//   bitstream - (in) the bitstream data in WebP RIFF format
//   copy_data - (in) value 1 indicates given data WILL be copied to the mux
//               object and value 0 indicates data will NOT be copied. If the
//               data is not copied, it must exist for the lifetime of the
//               mux object.
// Returns:
//   A pointer to the mux object created from given data - on success.
//   NULL - In case of invalid data or memory error.
CCGEN_WEBP_NODISCARD static CCGEN_WEBP_INLINE ccgen_WebPMux* ccgen_WebPMuxCreate(
    const ccgen_WebPData* bitstream, int copy_data) {
  return ccgen_WebPMuxCreateInternal(bitstream, copy_data, CCGEN_WEBP_MUX_ABI_VERSION);
}

//------------------------------------------------------------------------------
// Non-image chunks.

// Note: Only non-image related chunks should be managed through chunk APIs.
// (Image related chunks are: "ANMF", "VP8 ", "VP8L" and "ALPH").
// To add, get and delete images, use ccgen_WebPMuxSetImage(), ccgen_WebPMuxPushFrame(),
// ccgen_WebPMuxGetFrame() and ccgen_WebPMuxDeleteFrame().

// Adds a chunk with id 'fourcc' and data 'chunk_data' in the mux object.
// Any existing chunk(s) with the same id will be removed.
// Parameters:
//   mux - (in/out) object to which the chunk is to be added
//   fourcc - (in) a character array containing the fourcc of the given chunk;
//                 e.g., "ICCP", "XMP ", "EXIF" etc.
//   chunk_data - (in) the chunk data to be added
//   copy_data - (in) value 1 indicates given data WILL be copied to the mux
//               object and value 0 indicates data will NOT be copied. If the
//               data is not copied, it must exist until a call to
//               ccgen_WebPMuxAssemble() is made.
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux, fourcc or chunk_data is NULL
//                               or if fourcc corresponds to an image chunk.
//   CCGEN_WEBP_MUX_MEMORY_ERROR - on memory allocation error.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxSetChunk(ccgen_WebPMux* mux, const char fourcc[4],
                                         const ccgen_WebPData* chunk_data,
                                         int copy_data);

// Gets a reference to the data of the chunk with id 'fourcc' in the mux object.
// The caller should NOT free the returned data.
// Parameters:
//   mux - (in) object from which the chunk data is to be fetched
//   fourcc - (in) a character array containing the fourcc of the chunk;
//                 e.g., "ICCP", "XMP ", "EXIF" etc.
//   chunk_data - (out) returned chunk data
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux, fourcc or chunk_data is NULL
//                               or if fourcc corresponds to an image chunk.
//   CCGEN_WEBP_MUX_NOT_FOUND - If mux does not contain a chunk with the given id.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxGetChunk(const ccgen_WebPMux* mux,
                                         const char fourcc[4],
                                         ccgen_WebPData* chunk_data);

// Deletes the chunk with the given 'fourcc' from the mux object.
// Parameters:
//   mux - (in/out) object from which the chunk is to be deleted
//   fourcc - (in) a character array containing the fourcc of the chunk;
//                 e.g., "ICCP", "XMP ", "EXIF" etc.
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux or fourcc is NULL
//                               or if fourcc corresponds to an image chunk.
//   CCGEN_WEBP_MUX_NOT_FOUND - If mux does not contain a chunk with the given fourcc.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxDeleteChunk(ccgen_WebPMux* mux, const char fourcc[4]);

//------------------------------------------------------------------------------
// Images.

// Encapsulates data about a single frame.
struct ccgen_WebPMuxFrameInfo {
  ccgen_WebPData bitstream;  // image data: can be a raw VP8/VP8L bitstream
                       // or a single-image WebP file.
  int x_offset;        // x-offset of the frame.
  int y_offset;        // y-offset of the frame.
  int duration;        // duration of the frame (in milliseconds).

  ccgen_WebPChunkId id;  // frame type: should be one of CCGEN_WEBP_CHUNK_ANMF
                   // or CCGEN_WEBP_CHUNK_IMAGE
  ccgen_WebPMuxAnimDispose dispose_method;  // Disposal method for the frame.
  ccgen_WebPMuxAnimBlend blend_method;      // Blend operation for the frame.
  uint32_t pad[1];                    // padding for later use
};

// Sets the (non-animated) image in the mux object.
// Note: Any existing images (including frames) will be removed.
// Parameters:
//   mux - (in/out) object in which the image is to be set
//   bitstream - (in) can be a raw VP8/VP8L bitstream or a single-image
//               WebP file (non-animated)
//   copy_data - (in) value 1 indicates given data WILL be copied to the mux
//               object and value 0 indicates data will NOT be copied. If the
//               data is not copied, it must exist until a call to
//               ccgen_WebPMuxAssemble() is made.
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux is NULL or bitstream is NULL.
//   CCGEN_WEBP_MUX_MEMORY_ERROR - on memory allocation error.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxSetImage(ccgen_WebPMux* mux,
                                         const ccgen_WebPData* bitstream,
                                         int copy_data);

// Adds a frame at the end of the mux object.
// Notes: (1) frame.id should be CCGEN_WEBP_CHUNK_ANMF
//        (2) For setting a non-animated image, use ccgen_WebPMuxSetImage() instead.
//        (3) Type of frame being pushed must be same as the frames in mux.
//        (4) As WebP only supports even offsets, any odd offset will be snapped
//            to an even location using: offset &= ~1
// Parameters:
//   mux - (in/out) object to which the frame is to be added
//   frame - (in) frame data.
//   copy_data - (in) value 1 indicates given data WILL be copied to the mux
//               object and value 0 indicates data will NOT be copied. If the
//               data is not copied, it must exist until a call to
//               ccgen_WebPMuxAssemble() is made.
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux or frame is NULL
//                               or if content of 'frame' is invalid.
//   CCGEN_WEBP_MUX_MEMORY_ERROR - on memory allocation error.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxPushFrame(ccgen_WebPMux* mux,
                                          const ccgen_WebPMuxFrameInfo* frame,
                                          int copy_data);

// Gets the nth frame from the mux object.
// The content of 'frame->bitstream' is allocated using ccgen_WebPMalloc(), and NOT
// owned by the 'mux' object. It MUST be deallocated by the caller by calling
// ccgen_WebPDataClear().
// nth=0 has a special meaning - last position.
// Parameters:
//   mux - (in) object from which the info is to be fetched
//   nth - (in) index of the frame in the mux object
//   frame - (out) data of the returned frame
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux or frame is NULL.
//   CCGEN_WEBP_MUX_NOT_FOUND - if there are less than nth frames in the mux object.
//   CCGEN_WEBP_MUX_BAD_DATA - if nth frame chunk in mux is invalid.
//   CCGEN_WEBP_MUX_MEMORY_ERROR - on memory allocation error.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxGetFrame(const ccgen_WebPMux* mux, uint32_t nth,
                                         ccgen_WebPMuxFrameInfo* frame);

// Deletes a frame from the mux object.
// nth=0 has a special meaning - last position.
// Parameters:
//   mux - (in/out) object from which a frame is to be deleted
//   nth - (in) The position from which the frame is to be deleted
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux is NULL.
//   CCGEN_WEBP_MUX_NOT_FOUND - If there are less than nth frames in the mux object
//                        before deletion.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxDeleteFrame(ccgen_WebPMux* mux, uint32_t nth);

//------------------------------------------------------------------------------
// Animation.

// Animation parameters.
struct ccgen_WebPMuxAnimParams {
  uint32_t bgcolor;  // Background color of the canvas stored (in MSB order) as:
                     // Bits 00 to 07: Alpha.
                     // Bits 08 to 15: Red.
                     // Bits 16 to 23: Green.
                     // Bits 24 to 31: Blue.
  int loop_count;    // Number of times to repeat the animation [0 = infinite].
};

// Sets the animation parameters in the mux object. Any existing ANIM chunks
// will be removed.
// Parameters:
//   mux - (in/out) object in which ANIM chunk is to be set/added
//   params - (in) animation parameters.
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux or params is NULL.
//   CCGEN_WEBP_MUX_MEMORY_ERROR - on memory allocation error.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError
ccgen_WebPMuxSetAnimationParams(ccgen_WebPMux* mux, const ccgen_WebPMuxAnimParams* params);

// Gets the animation parameters from the mux object.
// Parameters:
//   mux - (in) object from which the animation parameters to be fetched
//   params - (out) animation parameters extracted from the ANIM chunk
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux or params is NULL.
//   CCGEN_WEBP_MUX_NOT_FOUND - if ANIM chunk is not present in mux object.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxGetAnimationParams(const ccgen_WebPMux* mux,
                                                   ccgen_WebPMuxAnimParams* params);

//------------------------------------------------------------------------------
// Misc Utilities.

// Sets the canvas size for the mux object. The width and height can be
// specified explicitly or left as zero (0, 0).
// * When width and height are specified explicitly, then this frame bound is
//   enforced during subsequent calls to ccgen_WebPMuxAssemble() and an error is
//   reported if any animated frame does not completely fit within the canvas.
// * When unspecified (0, 0), the constructed canvas will get the frame bounds
//   from the bounding-box over all frames after calling ccgen_WebPMuxAssemble().
// Parameters:
//   mux - (in) object to which the canvas size is to be set
//   width - (in) canvas width
//   height - (in) canvas height
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux is NULL; or
//                               width or height are invalid or out of bounds
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxSetCanvasSize(ccgen_WebPMux* mux, int width,
                                              int height);

// Gets the canvas size from the mux object.
// Note: This method assumes that the VP8X chunk, if present, is up-to-date.
// That is, the mux object hasn't been modified since the last call to
// ccgen_WebPMuxAssemble() or ccgen_WebPMuxCreate().
// Parameters:
//   mux - (in) object from which the canvas size is to be fetched
//   width - (out) canvas width
//   height - (out) canvas height
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux, width or height is NULL.
//   CCGEN_WEBP_MUX_BAD_DATA - if VP8X/VP8/VP8L chunk or canvas size is invalid.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxGetCanvasSize(const ccgen_WebPMux* mux, int* width,
                                              int* height);

// Gets the feature flags from the mux object.
// Note: This method assumes that the VP8X chunk, if present, is up-to-date.
// That is, the mux object hasn't been modified since the last call to
// ccgen_WebPMuxAssemble() or ccgen_WebPMuxCreate().
// Parameters:
//   mux - (in) object from which the features are to be fetched
//   flags - (out) the flags specifying which features are present in the
//           mux object. This will be an OR of various flag values.
//           Enum 'WebPFeatureFlags' can be used to test individual flag values.
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux or flags is NULL.
//   CCGEN_WEBP_MUX_BAD_DATA - if VP8X/VP8/VP8L chunk or canvas size is invalid.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxGetFeatures(const ccgen_WebPMux* mux,
                                            uint32_t* flags);

// Gets number of chunks with the given 'id' in the mux object.
// Parameters:
//   mux - (in) object from which the info is to be fetched
//   id - (in) chunk id specifying the type of chunk
//   num_elements - (out) number of chunks with the given chunk id
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux, or num_elements is NULL.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxNumChunks(const ccgen_WebPMux* mux, ccgen_WebPChunkId id,
                                          int* num_elements);

// Assembles all chunks in WebP RIFF format and returns in 'assembled_data'.
// This function also validates the mux object.
// Note: The content of 'assembled_data' will be ignored and overwritten.
// Also, the content of 'assembled_data' is allocated using ccgen_WebPMalloc(), and
// NOT owned by the 'mux' object. It MUST be deallocated by the caller by
// calling ccgen_WebPDataClear(). It's always safe to call ccgen_WebPDataClear() upon
// return, even in case of error.
// Parameters:
//   mux - (in/out) object whose chunks are to be assembled
//   assembled_data - (out) assembled WebP data
// Returns:
//   CCGEN_WEBP_MUX_BAD_DATA - if mux object is invalid.
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if mux or assembled_data is NULL.
//   CCGEN_WEBP_MUX_MEMORY_ERROR - on memory allocation error.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPMuxAssemble(ccgen_WebPMux* mux,
                                         ccgen_WebPData* assembled_data);

//------------------------------------------------------------------------------
// ccgen_WebPAnimEncoder API
//
// This API allows encoding (possibly) animated WebP images.
//
// Code Example:
/*
  ccgen_WebPAnimEncoderOptions enc_options;
  ccgen_WebPAnimEncoderOptionsInit(&enc_options);
  // Tune 'enc_options' as needed.
  ccgen_WebPAnimEncoder* enc = ccgen_WebPAnimEncoderNew(width, height, &enc_options);
  while(<there are more frames>) {
    ccgen_WebPConfig config;
    ccgen_WebPConfigInit(&config);
    // Tune 'config' as needed.
    ccgen_WebPAnimEncoderAdd(enc, frame, timestamp_ms, &config);
  }
  ccgen_WebPAnimEncoderAdd(enc, NULL, timestamp_ms, NULL);
  ccgen_WebPAnimEncoderAssemble(enc, webp_data);
  ccgen_WebPAnimEncoderDelete(enc);
  // Write the 'webp_data' to a file, or re-mux it further.
*/

typedef struct ccgen_WebPAnimEncoder ccgen_WebPAnimEncoder;  // Main opaque object.

// Forward declarations. Defined in encode.h.
struct ccgen_WebPPicture;
struct ccgen_WebPConfig;

// Global options.
struct ccgen_WebPAnimEncoderOptions {
  ccgen_WebPMuxAnimParams anim_params;  // Animation parameters.
  int minimize_size;  // If true, minimize the output size (slow). Implicitly
                      // disables key-frame insertion.
  int kmin;
  int kmax;         // Minimum and maximum distance between consecutive key
                    // frames in the output. The library may insert some key
                    // frames as needed to satisfy this criteria.
                    // Note that these conditions should hold: kmax > kmin
                    // and kmin >= kmax / 2 + 1. Also, if kmax <= 0, then
                    // key-frame insertion is disabled; and if kmax == 1,
                    // then all frames will be key-frames (kmin value does
                    // not matter for these special cases).
  int allow_mixed;  // If true, use mixed compression mode; may choose
                    // either lossy and lossless for each frame.
  int verbose;      // If true, print info and warning messages to stderr.

  uint32_t padding[4];  // Padding for later use.
};

// Internal, version-checked, entry point.
CCGEN_WEBP_EXTERN int ccgen_WebPAnimEncoderOptionsInitInternal(ccgen_WebPAnimEncoderOptions*,
                                                   int);

// Should always be called, to initialize a fresh ccgen_WebPAnimEncoderOptions
// structure before modification. Returns false in case of version mismatch.
// ccgen_WebPAnimEncoderOptionsInit() must have succeeded before using the
// 'enc_options' object.
CCGEN_WEBP_NODISCARD static CCGEN_WEBP_INLINE int ccgen_WebPAnimEncoderOptionsInit(
    ccgen_WebPAnimEncoderOptions* enc_options) {
  return ccgen_WebPAnimEncoderOptionsInitInternal(enc_options, CCGEN_WEBP_MUX_ABI_VERSION);
}

// Internal, version-checked, entry point.
CCGEN_WEBP_EXTERN ccgen_WebPAnimEncoder* ccgen_WebPAnimEncoderNewInternal(
    int, int, const ccgen_WebPAnimEncoderOptions*, int);

// Creates and initializes a ccgen_WebPAnimEncoder object.
// Parameters:
//   width/height - (in) canvas width and height of the animation.
//   enc_options - (in) encoding options; can be passed NULL to pick
//                      reasonable defaults.
// Returns:
//   A pointer to the newly created ccgen_WebPAnimEncoder object.
//   Or NULL in case of memory error.
static CCGEN_WEBP_INLINE ccgen_WebPAnimEncoder* ccgen_WebPAnimEncoderNew(
    int width, int height, const ccgen_WebPAnimEncoderOptions* enc_options) {
  return ccgen_WebPAnimEncoderNewInternal(width, height, enc_options,
                                    CCGEN_WEBP_MUX_ABI_VERSION);
}

// Optimize the given frame for WebP, encode it and add it to the
// ccgen_WebPAnimEncoder object.
// The last call to 'WebPAnimEncoderAdd' should be with frame = NULL, which
// indicates that no more frames are to be added. This call is also used to
// determine the duration of the last frame.
// Parameters:
//   enc - (in/out) object to which the frame is to be added.
//   frame - (in/out) frame data in ARGB or YUV(A) format. If it is in YUV(A)
//           format, it will be converted to ARGB, which incurs a small loss.
//   timestamp_ms - (in) timestamp of this frame in milliseconds.
//                       Duration of a frame would be calculated as
//                       "timestamp of next frame - timestamp of this frame".
//                       Hence, timestamps should be in non-decreasing order.
//   config - (in) encoding options; can be passed NULL to pick
//            reasonable defaults.
// Returns:
//   On error, returns false and frame->error_code is set appropriately.
//   Otherwise, returns true.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPAnimEncoderAdd(
    ccgen_WebPAnimEncoder* enc, struct ccgen_WebPPicture* frame, int timestamp_ms,
    const struct ccgen_WebPConfig* config);

// Assemble all frames added so far into a WebP bitstream.
// This call should be preceded by  a call to 'WebPAnimEncoderAdd' with
// frame = NULL; if not, the duration of the last frame will be internally
// estimated.
// Parameters:
//   enc - (in/out) object from which the frames are to be assembled.
//   webp_data - (out) generated WebP bitstream.
// Returns:
//   True on success.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN int ccgen_WebPAnimEncoderAssemble(ccgen_WebPAnimEncoder* enc,
                                                       ccgen_WebPData* webp_data);

// Get error string corresponding to the most recent call using 'enc'. The
// returned string is owned by 'enc' and is valid only until the next call to
// ccgen_WebPAnimEncoderAdd() or ccgen_WebPAnimEncoderAssemble() or ccgen_WebPAnimEncoderDelete().
// Parameters:
//   enc - (in/out) object from which the error string is to be fetched.
// Returns:
//   NULL if 'enc' is NULL. Otherwise, returns the error string if the last call
//   to 'enc' had an error, or an empty string if the last call was a success.
CCGEN_WEBP_EXTERN const char* ccgen_WebPAnimEncoderGetError(ccgen_WebPAnimEncoder* enc);

// Deletes the ccgen_WebPAnimEncoder object.
// Parameters:
//   enc - (in/out) object to be deleted
CCGEN_WEBP_EXTERN void ccgen_WebPAnimEncoderDelete(ccgen_WebPAnimEncoder* enc);

//------------------------------------------------------------------------------
// Non-image chunks.

// Note: Only non-image related chunks should be managed through chunk APIs.
// (Image related chunks are: "ANMF", "VP8 ", "VP8L" and "ALPH").

// Adds a chunk with id 'fourcc' and data 'chunk_data' in the enc object.
// Any existing chunk(s) with the same id will be removed.
// Parameters:
//   enc - (in/out) object to which the chunk is to be added
//   fourcc - (in) a character array containing the fourcc of the given chunk;
//                 e.g., "ICCP", "XMP ", "EXIF", etc.
//   chunk_data - (in) the chunk data to be added
//   copy_data - (in) value 1 indicates given data WILL be copied to the enc
//               object and value 0 indicates data will NOT be copied. If the
//               data is not copied, it must exist until a call to
//               ccgen_WebPAnimEncoderAssemble() is made.
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if enc, fourcc or chunk_data is NULL.
//   CCGEN_WEBP_MUX_MEMORY_ERROR - on memory allocation error.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPAnimEncoderSetChunk(ccgen_WebPAnimEncoder* enc,
                                                 const char fourcc[4],
                                                 const ccgen_WebPData* chunk_data,
                                                 int copy_data);

// Gets a reference to the data of the chunk with id 'fourcc' in the enc object.
// The caller should NOT free the returned data.
// Parameters:
//   enc - (in) object from which the chunk data is to be fetched
//   fourcc - (in) a character array containing the fourcc of the chunk;
//                 e.g., "ICCP", "XMP ", "EXIF", etc.
//   chunk_data - (out) returned chunk data
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if enc, fourcc or chunk_data is NULL.
//   CCGEN_WEBP_MUX_NOT_FOUND - If enc does not contain a chunk with the given id.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPAnimEncoderGetChunk(const ccgen_WebPAnimEncoder* enc,
                                                 const char fourcc[4],
                                                 ccgen_WebPData* chunk_data);

// Deletes the chunk with the given 'fourcc' from the enc object.
// Parameters:
//   enc - (in/out) object from which the chunk is to be deleted
//   fourcc - (in) a character array containing the fourcc of the chunk;
//                 e.g., "ICCP", "XMP ", "EXIF", etc.
// Returns:
//   CCGEN_WEBP_MUX_INVALID_ARGUMENT - if enc or fourcc is NULL.
//   CCGEN_WEBP_MUX_NOT_FOUND - If enc does not contain a chunk with the given fourcc.
//   CCGEN_WEBP_MUX_OK - on success.
CCGEN_WEBP_EXTERN ccgen_WebPMuxError ccgen_WebPAnimEncoderDeleteChunk(ccgen_WebPAnimEncoder* enc,
                                                    const char fourcc[4]);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // CCGEN_WEBP_MUX_H_
