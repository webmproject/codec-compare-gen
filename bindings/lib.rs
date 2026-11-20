#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPGetEncoderVersion() -> std::ffi::c_int {
    // The encoder relies on the Rust image crate.
    // TODO(b/509475659): Add some API to get the version number in
    //                    https://github.com/image-rs/image
    0x00190A // 0.25.10
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPGetDecoderVersion() -> std::ffi::c_int {
    // The decoder relies on the Rust image-webp crate. See ccgen_WebPAnimDecoderGetInfo().
    // TODO(b/509475659): Add some API to get the version number in
    //                    https://github.com/image-rs/image-webp
    0x000204 // 0.2.4
}

#[repr(C)]
#[derive(Default)]
pub struct ccgen_WebPConfig {
    pub lossless: std::ffi::c_int,
    pub quality: std::ffi::c_float,
    pub method: std::ffi::c_int,
    pub image_hint: std::ffi::c_int,
    pub target_size: std::ffi::c_int,
    pub target_psnr: std::ffi::c_float,
    pub segments: std::ffi::c_int,
    pub sns_strength: std::ffi::c_int,
    pub filter_strength: std::ffi::c_int,
    pub filter_sharpness: std::ffi::c_int,
    pub filter_type: std::ffi::c_int,
    pub autofilter: std::ffi::c_int,
    pub alpha_compression: std::ffi::c_int,
    pub alpha_filtering: std::ffi::c_int,
    pub alpha_quality: std::ffi::c_int,
    pub pass: std::ffi::c_int,
    pub show_compressed: std::ffi::c_int,
    pub preprocessing: std::ffi::c_int,
    pub partitions: std::ffi::c_int,
    pub partition_limit: std::ffi::c_int,
    pub emulate_jpeg_size: std::ffi::c_int,
    pub thread_level: std::ffi::c_int,
    pub low_memory: std::ffi::c_int,
    pub near_lossless: std::ffi::c_int,
    pub exact: std::ffi::c_int,
    pub use_delta_palette: std::ffi::c_int,
    pub use_sharp_yuv: std::ffi::c_int,
    pub qmin: std::ffi::c_int,
    pub qmax: std::ffi::c_int,
}

pub type ccgen_WebPWriterFunction = Option<
    extern "C" fn(
        data: *const u8,
        data_size: usize,
        picture: Option<&ccgen_WebPPicture>,
    ) -> std::ffi::c_int,
>;

pub type ccgen_WebPProgressHook = Option<
    extern "C" fn(percent: std::ffi::c_int, picture: Option<&ccgen_WebPPicture>) -> std::ffi::c_int,
>;

#[repr(C)]
#[derive(Default)]
pub struct ccgen_WebPPicture {
    pub use_argb: std::ffi::c_int,
    pub colorspace: std::ffi::c_int,
    pub width: std::ffi::c_int,
    pub height: std::ffi::c_int,
    pub y: *mut u8,
    pub u: *mut u8,
    pub v: *mut u8,
    pub y_stride: std::ffi::c_int,
    pub uv_stride: std::ffi::c_int,
    pub a: *mut u8,
    pub a_stride: std::ffi::c_int,
    pub pad1: [u32; 2],
    pub argb: *mut u32,
    pub argb_stride: std::ffi::c_int,
    pub pad2: [u32; 3],
    pub writer: ccgen_WebPWriterFunction,
    pub custom_ptr: *mut std::ffi::c_void,
    pub extra_info_type: std::ffi::c_int,
    pub extra_info: *mut u8,
    pub stats: *mut std::ffi::c_void,
    pub error_code: std::ffi::c_int,
    pub progress_hook: ccgen_WebPProgressHook,
    pub user_data: *mut std::ffi::c_void,
    pub pad3: [u32; 3],
    pub pad4: *mut u8,
    pub pad5: *mut u8,
    pub pad6: [u32; 8],
    pub memory_: *mut std::ffi::c_void,
    pub memory_argb_: *mut std::ffi::c_void,
    pub pad7: [*mut std::ffi::c_void; 2],
}

#[repr(C)]
pub struct ccgen_WebPData {
    pub bytes: *const u8,
    pub size: usize,
}

#[repr(C)]
#[derive(Default)]
pub struct ccgen_WebPMuxAnimParams {
    pub bgcolor: u32,
    pub loop_count: std::ffi::c_int,
}

#[repr(C)]
#[derive(Default)]
pub struct ccgen_WebPAnimEncoderOptions {
    pub anim_params: ccgen_WebPMuxAnimParams,
    pub minimize_size: std::ffi::c_int,
    pub kmin: std::ffi::c_int,
    pub kmax: std::ffi::c_int,
    pub allow_mixed: std::ffi::c_int,
    pub verbose: std::ffi::c_int,
    pub padding: [u32; 4],
}

#[repr(C)]
#[derive(Default)]
pub struct ccgen_WebPAnimDecoderOptions {
    pub color_mode: std::ffi::c_int,
    pub use_threads: std::ffi::c_int,
    pub padding: [u32; 7],
}

#[repr(C)]
pub struct ccgen_WebPAnimInfo {
    pub canvas_width: u32,
    pub canvas_height: u32,
    pub loop_count: u32,
    pub bgcolor: u32,
    pub frame_count: u32,
    pub pad: [u32; 4],
}

pub struct ccgen_WebPAnimEncoder {}

pub struct ccgen_WebPAnimDecoder<'a> {
    decoder: Box<image_webp::WebPDecoder<std::io::Cursor<&'a [u8]>>>,
    next_frame_index: u32,
    next_frame_timestamp_ms: u32,
    buffer: Vec<u8>,
}

const ERROR: std::ffi::c_int = 0;
const OK: std::ffi::c_int = 1;

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPPictureInitInternal(
    picture: Option<&mut ccgen_WebPPicture>,
    _version: std::ffi::c_int,
) -> std::ffi::c_int {
    if let Some(picture) = picture {
        *picture = ccgen_WebPPicture::default();
        OK
    } else {
        ERROR
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPPictureFree(picture: Option<&mut ccgen_WebPPicture>) {
    if let Some(picture) = picture {
        // ccgen_WebPPicture should not own any memory.
        // TODO(b/509475659): Revisit this assumption if the whole libwebp API is implemented.
        if !picture.memory_.is_null() || !picture.memory_argb_.is_null() {
            return;
        }

        picture.y = std::ptr::null_mut();
        picture.u = std::ptr::null_mut();
        picture.v = std::ptr::null_mut();
        picture.a = std::ptr::null_mut();
        picture.argb = std::ptr::null_mut();
    }
}

// The Rust image crate does not support encoding lossy WebP. Still implement this function
// for API parity with libwebp.
#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPConfigInitInternal(
    config: Option<&mut ccgen_WebPConfig>,
    _preset: std::ffi::c_int,
    quality: std::ffi::c_float,
    _version: std::ffi::c_int,
) -> std::ffi::c_int {
    if let Some(config) = config {
        *config = ccgen_WebPConfig::default();
        config.quality = quality;
        config.method = 4;
        OK
    } else {
        ERROR
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPConfigLosslessPreset(
    config: Option<&mut ccgen_WebPConfig>,
    level: std::ffi::c_int,
) -> std::ffi::c_int {
    if let Some(config) = config {
        config.lossless = 1;
        config.method = level;
        config.quality = 100.0;
        OK
    } else {
        ERROR
    }
}

/// Encodes a WebP image from a `ccgen_WebPPicture`.
///
/// # Safety
/// If `config` or `picture` is `None`, lossless ARGB encoding is not configured
/// (`config.lossless == 0`, `config.quality != 100.0`, `config.method` outside `0..=9`, or
/// `picture.use_argb == 0`), `picture.width` or `picture.height` is outside `1..=16383`,
/// `picture.argb_stride < picture.width`, `picture.argb` is null or not 4-byte aligned, or
/// `(picture.argb_stride as usize) * (picture.height as usize) * size_of::<u32>()` overflows
/// `usize` or exceeds `isize::MAX as usize`, this function returns `ERROR` (`0`) without
/// dereferencing `picture.argb`.
///
/// Otherwise, the caller must ensure that `picture.argb` lies within a single live allocation, is
/// valid for reads of `(picture.argb_stride as usize) * (picture.height as usize)` consecutive
/// initialized `u32` elements, and is not mutated while `ccgen_WebPEncode` reads it.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ccgen_WebPEncode(
    config: Option<&ccgen_WebPConfig>,
    picture: Option<&mut ccgen_WebPPicture>,
) -> std::ffi::c_int {
    if let (Some(config), Some(picture)) = (config, picture) {
        // The Rust image crate does not support encoding lossy WebP.
        if config.lossless == 0 || config.quality != 100.0 {
            picture.error_code = 4; // VP8_ENC_ERROR_INVALID_CONFIGURATION
            return ERROR;
        }
        // The value of config.method is ignored because the Rust image crate has no effort parameter.
        // Still sanity check it for API parity with libwebp.
        if config.method < 0 || config.method > 9 {
            picture.error_code = 4; // VP8_ENC_ERROR_INVALID_CONFIGURATION
            return ERROR;
        }

        if picture.use_argb == 0 {
            // Not implemented.
            picture.error_code = 4; // VP8_ENC_ERROR_INVALID_CONFIGURATION
            return ERROR;
        }
        // TODO(b/509475659): Check for other unsupported parameters (picture.progress_hook, etc.).

        if picture.width <= 0
            || picture.height <= 0
            || picture.width > 16383
            || picture.height > 16383
            || picture.argb_stride < picture.width
        {
            picture.error_code = 5; // VP8_ENC_ERROR_BAD_DIMENSION
            return ERROR;
        }
        if picture.argb.is_null() {
            picture.error_code = 3; // VP8_ENC_ERROR_NULL_PARAMETER
            return ERROR;
        }
        if (picture.argb as usize) % std::mem::align_of::<u32>() != 0 {
            picture.error_code = 4; // VP8_ENC_ERROR_INVALID_CONFIGURATION
            return ERROR;
        }
        let width = picture.width as usize;
        let height = picture.height as usize;
        let stride = picture.argb_stride as usize; // In pixels, not bytes.
        let Some(argb_size) = stride.checked_mul(height) else {
            picture.error_code = 5; // VP8_ENC_ERROR_BAD_DIMENSION
            return ERROR;
        };
        match argb_size.checked_mul(std::mem::size_of::<u32>()) {
            Some(bytes) if bytes <= isize::MAX as usize => {}
            _ => {
                picture.error_code = 5; // VP8_ENC_ERROR_BAD_DIMENSION
                return ERROR;
            }
        }
        let Some(rgba8_len) = width.checked_mul(height).and_then(|n| n.checked_mul(4)) else {
            picture.error_code = 5; // VP8_ENC_ERROR_BAD_DIMENSION
            return ERROR;
        };

        // image::ExtendedColorType::Bgra8 is not supported by image::codecs::webp::WebPEncoder.
        // Convert from ccgen_WebPPicture's little-endian 32-bit ARGB (8-bit BGRA) to 8-bit RGBA.
        // SAFETY: `picture.argb` is checked above to be non-null and aligned to
        //         `align_of::<u32>()`, and `argb_size * size_of::<u32>() <= isize::MAX`. Per the
        //         `# Safety` contract of `ccgen_WebPEncode`, the caller guarantees that
        //         `picture.argb` lies within a single live allocation, is valid for reads of
        //         `argb_size` consecutive initialized `u32` elements, and is not mutated for the
        //         lifetime of `argb32`.
        let argb32 = unsafe { std::slice::from_raw_parts(picture.argb as *const u32, argb_size) };
        let mut rgba8 = Vec::new();
        if rgba8.try_reserve_exact(rgba8_len).is_err() {
            picture.error_code = 1; // VP8_ENC_ERROR_OUT_OF_MEMORY
            return ERROR;
        }
        for row in 0..height {
            let row_start = row * stride;
            for col in 0..width {
                let bgra8 = argb32[row_start + col].to_le_bytes();
                let (b, g, r, a) = (bgra8[0], bgra8[1], bgra8[2], bgra8[3]);
                rgba8.extend_from_slice(&[r, g, b, a]);
            }
        }

        let mut out_vec = Vec::new();
        let encoder = image::codecs::webp::WebPEncoder::new_lossless(&mut out_vec);
        match encoder.encode(
            &rgba8,
            width as u32,
            height as u32,
            image::ExtendedColorType::Rgba8,
        ) {
            Ok(()) => {
                if let Some(writer) = picture.writer {
                    if writer(out_vec.as_ptr(), out_vec.len(), Some(picture)) != 1 {
                        picture.error_code = 8; // VP8_ENC_ERROR_BAD_WRITE
                        return ERROR;
                    }
                    OK
                } else {
                    picture.error_code = 3; // VP8_ENC_ERROR_NULL_PARAMETER
                    ERROR
                }
            }
            Err(_) => {
                picture.error_code = 4; // Probably VP8_ENC_ERROR_INVALID_CONFIGURATION, maybe OOM
                ERROR
            }
        }
    } else {
        ERROR
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPAnimEncoderOptionsInitInternal(
    options: Option<&mut ccgen_WebPAnimEncoderOptions>,
    _version: std::ffi::c_int,
) -> std::ffi::c_int {
    if let Some(options) = options {
        *options = ccgen_WebPAnimEncoderOptions::default();
        OK
    } else {
        ERROR
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPAnimEncoderNewInternal(
    _width: std::ffi::c_int,
    _height: std::ffi::c_int,
    _enc_options: Option<&ccgen_WebPAnimEncoderOptions>,
    _version: std::ffi::c_int,
) -> *mut ccgen_WebPAnimEncoder {
    // The Rust image crate does not support encoding animated WebP.
    std::ptr::null_mut()
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPAnimEncoderAdd(
    _enc: Option<&mut ccgen_WebPAnimEncoder>,
    _frame: Option<&mut ccgen_WebPPicture>,
    _timestamp_ms: std::ffi::c_int,
    _config: Option<&ccgen_WebPConfig>,
) -> std::ffi::c_int {
    // The Rust image crate does not support encoding animated WebP.
    ERROR
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPAnimEncoderAssemble(
    _enc: Option<&mut ccgen_WebPAnimEncoder>,
    _webp_data: Option<&mut ccgen_WebPData>,
) -> std::ffi::c_int {
    // The Rust image crate does not support encoding animated WebP.
    ERROR
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPAnimEncoderDelete(_enc: Option<&mut ccgen_WebPAnimEncoder>) {}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPAnimDecoderOptionsInitInternal(
    options: Option<&mut ccgen_WebPAnimDecoderOptions>,
    _version: std::ffi::c_int,
) -> std::ffi::c_int {
    if let Some(options) = options {
        *options = ccgen_WebPAnimDecoderOptions::default();
        options.color_mode = 1; // MODE_RGBA, see ccgen_WebPAnimDecoderNewInternal().
        OK
    } else {
        ERROR
    }
}

/// Creates a new `ccgen_WebPAnimDecoder`.
///
/// # Safety
/// If `webp_data` is `None`, `dec_options` specifies unsupported options (`color_mode != 1` or
/// `use_threads != 0`), `webp_data.bytes` is null, `webp_data.size == 0`, or
/// `webp_data.size > isize::MAX as usize`, this function returns null without dereferencing
/// `webp_data.bytes`.
///
/// Otherwise, the caller must ensure that:
/// - `webp_data.bytes` lies within a single live allocation and is valid for reads of
///   `webp_data.size` consecutive initialized `u8` bytes.
/// - If this function returns a non-null pointer, `webp_data.bytes[..webp_data.size]` remains
///   live, allocated, and unmutated until `ccgen_WebPAnimDecoderDelete` is called on the returned
///   decoder (or for the duration of this call if it returns null), because the returned
///   `ccgen_WebPAnimDecoder<'static>` borrows that slice internally.
///
/// If a non-null pointer is returned, the caller owns the decoder and must free it once via
/// `ccgen_WebPAnimDecoderDelete`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ccgen_WebPAnimDecoderNewInternal(
    webp_data: Option<&ccgen_WebPData>,
    dec_options: Option<&ccgen_WebPAnimDecoderOptions>,
    _version: std::ffi::c_int,
) -> *mut ccgen_WebPAnimDecoder<'static> {
    if let Some(webp_data) = webp_data {
        if let Some(dec_options) = dec_options {
            if dec_options.color_mode != 1 {
                return std::ptr::null_mut(); // Only MODE_RGBA is supported.
            }
            if dec_options.use_threads != 0 {
                return std::ptr::null_mut();
            }
        }
        if webp_data.bytes.is_null()
            || webp_data.size == 0
            || webp_data.size > isize::MAX as usize
        {
            return std::ptr::null_mut();
        }
        // SAFETY: `webp_data.bytes` is checked above to be non-null, `align_of::<u8>() == 1`,
        //         and `webp_data.size <= isize::MAX`. Per the `# Safety` contract of
        //         `ccgen_WebPAnimDecoderNewInternal`, the caller guarantees that
        //         `webp_data.bytes` lies within a single live allocation and is valid for reads
        //         of `webp_data.size` consecutive initialized `u8` bytes. Extending the slice
        //         lifetime to `'static` is sound because `slice` is stored only in private fields
        //         of `ccgen_WebPAnimDecoder`, never escapes the struct, and is dropped on `Err` or
        //         when `ccgen_WebPAnimDecoderDelete` drops the decoder; per `# Safety`, the caller
        //         guarantees `webp_data.bytes[..webp_data.size]` remains live, allocated, and
        //         unmutated until `ccgen_WebPAnimDecoderDelete` completes (or until this call
        //         returns if it returns null).
        let slice = unsafe { std::slice::from_raw_parts(webp_data.bytes, webp_data.size) };
        let cursor = std::io::Cursor::new(slice);
        match image_webp::WebPDecoder::new(cursor) {
            Ok(decoder) => Box::into_raw(Box::new(ccgen_WebPAnimDecoder {
                decoder: Box::new(decoder),
                next_frame_index: 0,
                next_frame_timestamp_ms: 0,
                buffer: vec![],
            })),
            Err(_) => std::ptr::null_mut(),
        }
    } else {
        std::ptr::null_mut()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPAnimDecoderGetInfo(
    dec: Option<&ccgen_WebPAnimDecoder>,
    info: Option<&mut ccgen_WebPAnimInfo>,
) -> std::ffi::c_int {
    if let (Some(dec), Some(info)) = (dec, info) {
        let decoder = &dec.decoder;
        (info.canvas_width, info.canvas_height) = decoder.dimensions();
        info.loop_count = match decoder.loop_count() {
            image_webp::LoopCount::Forever => 0,
            image_webp::LoopCount::Times(non_zero) => non_zero.get().into(),
        };
        // The image crate does not expose the background color.
        // The image crate does not expose the total number of frames without decoding everything.
        // This is why the decoding side relies on the image-webp crate directly.
        info.bgcolor = match decoder.background_color_hint() {
            Some(color) => u32::from_be_bytes(color),
            None => 0,
        };
        info.frame_count = decoder.num_frames();
        OK
    } else {
        ERROR
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPAnimDecoderHasMoreFrames(
    dec: Option<&ccgen_WebPAnimDecoder>,
) -> std::ffi::c_int {
    if let Some(dec) = dec {
        if dec.next_frame_index == 0 {
            true.into() // Still image or first frame of animation.
        } else if dec.next_frame_index < dec.decoder.num_frames() {
            true.into() // Frame of animation.
        } else {
            false.into() // The still image or the whole animation has been decoded already.
        }
    } else {
        false.into() // Error.
    }
}

/// Gets the next frame of an animated WebP file.
///
/// # Safety
/// `dec` must point to a valid `ccgen_WebPAnimDecoder`.
/// `buf` must point to a valid `*mut u8` pointer to be filled with the address of the frame buffer
/// which will stay valid until the next call to `ccgen_WebPAnimDecoderGetNext()` or
/// `ccgen_WebPAnimDecoderDelete()`.
/// `timestamp` must point to a valid `std::ffi::c_int` to be filled with the frame timestamp.
#[unsafe(no_mangle)]
pub extern "C" fn ccgen_WebPAnimDecoderGetNext(
    dec: Option<&mut ccgen_WebPAnimDecoder>,
    buf: Option<&mut *mut u8>,
    timestamp: Option<&mut std::ffi::c_int>,
) -> std::ffi::c_int {
    if let (Some(dec), Some(buf), Some(timestamp)) = (dec, buf, timestamp) {
        let decoder = &mut dec.decoder;
        let (width, height) = decoder.dimensions();
        if width == 0 || height == 0 {
            return ERROR;
        }
        let Some(num_pixels) = usize::try_from(width)
            .ok()
            .and_then(|w| usize::try_from(height).ok().and_then(|h| w.checked_mul(h)))
        else {
            return ERROR;
        };
        let Some(buffer_len) = num_pixels.checked_mul(4) else {
            return ERROR;
        };
        let expected_buffer_len = num_pixels * if decoder.has_alpha() { 4 } else { 3 };
        // dec.buffer will contain RGBA samples in this order.
        if dec.buffer.is_empty() {
            if dec.buffer.try_reserve_exact(buffer_len).is_err() {
                return ERROR;
            }
            dec.buffer.resize(buffer_len, 0); // Initial values do not matter.
        } else if dec.buffer.len() != buffer_len {
            return ERROR;
        }

        let delay_ms = if decoder.is_animated() {
            if dec.next_frame_index >= decoder.num_frames() {
                return ERROR;
            }
            // image_webp::WebPDecoder::read_frame() will fill the given buffer with RGBA samples
            // if image_webp::WebPDecoder::has_alpha() is true, and RGB samples otherwise.
            match decoder.read_frame(&mut dec.buffer[..expected_buffer_len]) {
                Ok(delay_ms) => delay_ms,
                Err(_) => return ERROR,
            }
        } else {
            if dec.next_frame_index >= 1 {
                return ERROR;
            }
            // image_webp::WebPDecoder::read_image() will fill the given buffer with RGBA samples
            // if image_webp::WebPDecoder::has_alpha() is true, and RGB samples otherwise.
            if decoder
                .read_image(&mut dec.buffer[..expected_buffer_len])
                .is_err()
            {
                return ERROR;
            }
            0 // Undefined delay_ms for a still image.
        };

        if !decoder.has_alpha() {
            // Insert opaque alpha values to always return RGBA samples (MODE_RGBA is expected).
            for i in (0..num_pixels).rev() {
                dec.buffer[4 * i + 3] = 255;
                dec.buffer[4 * i + 2] = dec.buffer[3 * i + 2];
                dec.buffer[4 * i + 1] = dec.buffer[3 * i + 1];
                dec.buffer[4 * i + 0] = dec.buffer[3 * i + 0];
            }
        }

        dec.next_frame_index = dec.next_frame_index.saturating_add(1);
        dec.next_frame_timestamp_ms = dec.next_frame_timestamp_ms.saturating_add(delay_ms);
        *buf = dec.buffer.as_mut_ptr();
        *timestamp =
            std::ffi::c_int::try_from(dec.next_frame_timestamp_ms).unwrap_or(std::ffi::c_int::MAX);
        OK
    } else {
        ERROR
    }
}

/// Deletes a `ccgen_WebPAnimDecoder`.
///
/// # Safety
/// If `dec` is null, this function is a safe no-op. Otherwise, the caller must ensure that `dec`
/// was returned by `ccgen_WebPAnimDecoderNewInternal()`, is not aliased by any other live
/// reference or pointer, and has not already been deleted. After this call returns, `dec` is
/// deallocated and dangling and must not be used again.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ccgen_WebPAnimDecoderDelete(dec: *mut ccgen_WebPAnimDecoder) {
    if !dec.is_null() {
        // SAFETY: `dec` is checked against null above. Per `# Safety`, `dec` was produced by
        //         `Box::into_raw()` in `ccgen_WebPAnimDecoderNewInternal()`, has unique ownership,
        //         and has not already been freed. Reconstructing and immediately dropping the
        //         `Box` deallocates the decoder and renders `dec` dangling.
        let _ = unsafe { Box::from_raw(dec) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_imagejpeg_encoder_version() -> std::ffi::c_int {
    0x00190A // 0.25.10
}

#[unsafe(no_mangle)]
pub extern "C" fn ccgen_zunejpeg_version() -> std::ffi::c_int {
    0x00050F // 0.5.15
}

/// Encodes an RGB_24 image to JPEG using the Rust image crate.
///
/// # Safety
/// If `output_bytes` or `output_size` is `None`, `rgb_pixels` is null, `width == 0`,
/// `height == 0`, `quality` is outside `1..=100`, `width` or `height` exceeds `u32::MAX`,
/// `stride != width * 3` (including overflow), or `stride * height` overflows `usize` or exceeds
/// `isize::MAX as usize`, this function returns `ERROR` (`0`) without reading `rgb_pixels`.
///
/// Otherwise, the caller must ensure that `rgb_pixels` lies within a single live allocation, is
/// valid for reads of `stride * height` consecutive initialized `u8` bytes, and is not mutated for
/// the duration of this call.
///
/// `output_bytes` and `output_size` are write-only out-parameters that are never read before being
/// overwritten on success, so callers may pass uninitialized storage. On `OK` (`1`),
/// `*output_bytes` points to a buffer of length `*output_size` allocated via
/// `Box::<[u8]>::into_raw` that the caller owns and must free once via
/// `ccgen_imagejpeg_free_buffer(*output_bytes, *output_size)`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ccgen_imagejpeg_encode444(
    rgb_pixels: *const u8,
    width: usize,
    height: usize,
    stride: usize,
    quality: std::ffi::c_int,
    output_bytes: Option<&mut *mut u8>,
    output_size: Option<&mut usize>,
) -> std::ffi::c_int {
    if let (Some(output_bytes), Some(output_size)) = (output_bytes, output_size) {
        if rgb_pixels.is_null() || width == 0 || height == 0 {
            return ERROR;
        }
        if quality < 1 || quality > 100 {
            return ERROR;
        }
        let (Ok(width_u32), Ok(height_u32)) = (u32::try_from(width), u32::try_from(height)) else {
            return ERROR;
        };
        let Some(expected_stride) = width.checked_mul(3) else {
            return ERROR;
        };
        if stride != expected_stride {
            return ERROR;
        }
        let Some(slice_len) = stride.checked_mul(height) else {
            return ERROR;
        };
        if slice_len > isize::MAX as usize {
            return ERROR;
        }
        // SAFETY: `rgb_pixels` is checked above to be non-null, `align_of::<u8>() == 1`, and
        //         `slice_len <= isize::MAX`. Per the `# Safety` contract of
        //         `ccgen_imagejpeg_encode444`, the caller guarantees that `rgb_pixels` lies within
        //         a single live allocation, is valid for reads of `slice_len` consecutive
        //         initialized `u8` bytes, and is not mutated for the lifetime of `slice`.
        let slice = unsafe { std::slice::from_raw_parts(rgb_pixels, slice_len) };

        let mut out_vec = Vec::new();
        let mut encoder =
            image::codecs::jpeg::JpegEncoder::new_with_quality(&mut out_vec, quality as u8);
        match encoder.encode(
            slice,
            width_u32,
            height_u32,
            image::ExtendedColorType::Rgb8,
        ) {
            Ok(()) => {
                let boxed_slice = out_vec.into_boxed_slice();
                *output_size = boxed_slice.len();
                *output_bytes = Box::into_raw(boxed_slice) as *mut u8;
                OK
            }
            Err(_) => ERROR,
        }
    } else {
        ERROR
    }
}

/// Decodes a JPEG image to RGB_24 using the Rust zune-jpeg crate.
///
/// # Safety
/// If `width`, `height`, or `output_bytes` is `None`, `encoded_bytes` is null,
/// `encoded_size == 0`, or `encoded_size > isize::MAX as usize`, this function returns `ERROR`
/// (`0`) without reading `encoded_bytes`.
///
/// Otherwise, the caller must ensure that `encoded_bytes` lies within a single live allocation, is
/// valid for reads of `encoded_size` consecutive initialized `u8` bytes, and is not mutated for
/// the duration of this call.
///
/// `width`, `height`, and `output_bytes` are write-only out-parameters that are never read before
/// being overwritten on success, so callers may pass uninitialized storage. On `OK` (`1`),
/// `*output_bytes` points to a buffer of length `*width * *height * 3` allocated via
/// `Box::<[u8]>::into_raw` that the caller owns and must free once via
/// `ccgen_imagejpeg_free_buffer(*output_bytes, *width * *height * 3)`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ccgen_zunejpeg_decode(
    encoded_bytes: *const u8,
    encoded_size: usize,
    width: Option<&mut usize>,
    height: Option<&mut usize>,
    output_bytes: Option<&mut *mut u8>,
) -> std::ffi::c_int {
    if let (Some(width), Some(height), Some(output_bytes)) = (width, height, output_bytes) {
        if encoded_bytes.is_null() || encoded_size == 0 || encoded_size > isize::MAX as usize {
            return ERROR;
        }
        // SAFETY: `encoded_bytes` is checked above to be non-null, `align_of::<u8>() == 1`, and
        //         `encoded_size <= isize::MAX`. Per the `# Safety` contract of
        //         `ccgen_zunejpeg_decode`, the caller guarantees that `encoded_bytes` lies within
        //         a single live allocation, is valid for reads of `encoded_size` consecutive
        //         initialized `u8` bytes, and is not mutated for the lifetime of `slice`.
        let slice = unsafe { std::slice::from_raw_parts(encoded_bytes, encoded_size) };
        let cursor = zune_core::bytestream::ZCursor::new(slice);
        let options = zune_core::options::DecoderOptions::default()
            .jpeg_set_out_colorspace(zune_core::colorspace::ColorSpace::RGB);
        let mut decoder = zune_jpeg::JpegDecoder::new_with_options(cursor, options);
        match decoder.decode() {
            Ok(pixels) => {
                if let Some(info) = decoder.info() {
                    let w = info.width as usize;
                    let h = info.height as usize;
                    let expected_len = w.checked_mul(h).and_then(|n| n.checked_mul(3));
                    if w == 0 || h == 0 || expected_len != Some(pixels.len()) {
                        return ERROR;
                    }
                    *width = w;
                    *height = h;
                    let boxed_slice = pixels.into_boxed_slice();
                    *output_bytes = Box::into_raw(boxed_slice) as *mut u8;
                    OK
                } else {
                    ERROR
                }
            }
            Err(_) => ERROR,
        }
    } else {
        ERROR
    }
}

/// Frees a buffer allocated by `ccgen_imagejpeg_encode444` or `ccgen_zunejpeg_decode`.
///
/// # Safety
/// If `buffer` is null or `size > isize::MAX as usize`, this function is a safe no-op.
///
/// Otherwise, the caller must ensure that:
/// - `(buffer, size)` was produced by `Box::<[u8]>::into_raw` (for example, returned by
///   `ccgen_imagejpeg_encode444` or `ccgen_zunejpeg_decode`) using this library's global
///   allocator, with exact `std::alloc::Layout::array::<u8>(size)` and `size` initialized `u8`
///   bytes (or, if `size == 0`, the non-null dangling pointer produced by `Box::<[u8]>::into_raw`
///   for an empty slice).
/// - `buffer[..size]` is not aliased by any other live reference or pointer and has not already
///   been freed.
///
/// After this call returns, the buffer is deallocated and `buffer` is dangling and must not be
/// used again.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ccgen_imagejpeg_free_buffer(buffer: *mut u8, size: usize) {
    if !buffer.is_null() && size <= isize::MAX as usize {
        // SAFETY: `buffer` is checked against null above, `align_of::<u8>() == 1`, and
        //         `size <= isize::MAX`. Per the `# Safety` contract of
        //         `ccgen_imagejpeg_free_buffer`, `(buffer, size)` came from
        //         `Box::<[u8]>::into_raw` on the global allocator with exact
        //         `Layout::array::<u8>(size)` and `size` initialized `u8` bytes (or `size == 0`
        //         with `Box::<[u8]>::into_raw`'s non-null dangling pointer), has
        //         unique/non-aliased ownership, and has not already been freed. Reconstructing and
        //         immediately dropping the `Box<[u8]>` deallocates the buffer and renders `buffer`
        //         dangling.
        let _ = unsafe { Box::from_raw(std::ptr::slice_from_raw_parts_mut(buffer, size)) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn anim_decoder_saturates_cumulative_timestamp() {
        const NUM_FRAMES: usize = 130;
        const MAX_DURATION: u32 = 0x00ff_ffff;
        let webp = include_bytes!("../tests/data/anim1x1_timestamp_overflow.webp");
        let data = ccgen_WebPData {
            bytes: webp.as_ptr(),
            size: webp.len(),
        };
        let mut options = ccgen_WebPAnimDecoderOptions::default();
        assert_eq!(ccgen_WebPAnimDecoderOptionsInitInternal(Some(&mut options), 0), OK);

        // SAFETY: `webp` stays alive until `ccgen_WebPAnimDecoderDelete` is called below.
        let dec = unsafe { ccgen_WebPAnimDecoderNewInternal(Some(&data), Some(&options), 0) };
        assert!(!dec.is_null());

        {
            // SAFETY: `dec` is a valid non-null pointer returned by
            //         `ccgen_WebPAnimDecoderNewInternal` and is not aliased during this block.
            let dec_ref = unsafe { &mut *dec };
            let mut info = ccgen_WebPAnimInfo {
                canvas_width: 0,
                canvas_height: 0,
                loop_count: 0,
                bgcolor: 0,
                frame_count: 0,
                pad: [0; 4],
            };
            assert_eq!(ccgen_WebPAnimDecoderGetInfo(Some(dec_ref), Some(&mut info)), OK);
            assert_eq!(info.canvas_width, 1);
            assert_eq!(info.canvas_height, 1);
            assert_eq!(info.frame_count, NUM_FRAMES as u32);

            for i in 0..NUM_FRAMES {
                assert_eq!(ccgen_WebPAnimDecoderHasMoreFrames(Some(dec_ref)), 1);
                let mut buf = std::ptr::null_mut();
                let mut timestamp: std::ffi::c_int = -1;
                assert_eq!(
                    ccgen_WebPAnimDecoderGetNext(
                        Some(dec_ref),
                        Some(&mut buf),
                        Some(&mut timestamp),
                    ),
                    OK
                );
                assert!(!buf.is_null());
                let expected = ((i as u64 + 1) * MAX_DURATION as u64)
                    .min(std::ffi::c_int::MAX as u64) as std::ffi::c_int;
                assert_eq!(timestamp, expected);
            }
            assert_eq!(ccgen_WebPAnimDecoderHasMoreFrames(Some(dec_ref)), 0);
        }

        // SAFETY: `dec` was allocated by `ccgen_WebPAnimDecoderNewInternal` and is freed once.
        unsafe { ccgen_WebPAnimDecoderDelete(dec) };
    }

    #[test]
    fn picture_and_config_init_and_free() {
        let mut sentinel: u8 = 0;
        let mut argb_sentinel: u32 = 0;
        let mut picture = ccgen_WebPPicture::default();
        picture.width = 42;
        assert_eq!(ccgen_WebPPictureInitInternal(Some(&mut picture), 0), OK);
        assert_eq!(picture.width, 0);
        assert_eq!(ccgen_WebPPictureInitInternal(None, 0), ERROR);

        picture.y = &mut sentinel;
        picture.u = &mut sentinel;
        picture.v = &mut sentinel;
        picture.a = &mut sentinel;
        picture.argb = &mut argb_sentinel;
        ccgen_WebPPictureFree(Some(&mut picture));
        assert!(picture.y.is_null());
        assert!(picture.u.is_null());
        assert!(picture.v.is_null());
        assert!(picture.a.is_null());
        assert!(picture.argb.is_null());

        picture.y = &mut sentinel;
        picture.memory_ = &mut sentinel as *mut u8 as *mut std::ffi::c_void;
        picture.memory_argb_ = &mut sentinel as *mut u8 as *mut std::ffi::c_void;
        ccgen_WebPPictureFree(Some(&mut picture));
        assert!(!picture.y.is_null());

        let mut config = ccgen_WebPConfig::default();
        config.lossless = 1;
        assert_eq!(ccgen_WebPConfigInitInternal(Some(&mut config), 0, 75.0, 0), OK);
        assert_eq!(config.lossless, 0);
        assert_eq!(config.quality, 75.0);
        assert_eq!(config.method, 4);
        assert_eq!(ccgen_WebPConfigInitInternal(None, 0, 75.0, 0), ERROR);
    }

    #[test]
    fn imagejpeg_encode_decode_free_roundtrip() {
        const W: usize = 2;
        const H: usize = 2;
        let rgb: [u8; W * H * 3] = [128; W * H * 3];
        let mut encoded_bytes = std::ptr::null_mut();
        let mut encoded_size = 0;

        // SAFETY: `rgb` is a single live allocation of `W * H * 3` initialized `u8` bytes,
        //         unmutated for the duration of the call, and `encoded_bytes` / `encoded_size` are
        //         valid mutable out-parameters.
        let ret = unsafe {
            ccgen_imagejpeg_encode444(
                rgb.as_ptr(),
                W,
                H,
                W * 3,
                90,
                Some(&mut encoded_bytes),
                Some(&mut encoded_size),
            )
        };
        assert_eq!(ret, OK);
        assert!(!encoded_bytes.is_null());
        assert!(encoded_size > 0);

        let mut decoded_w = 0;
        let mut decoded_h = 0;
        let mut decoded_bytes = std::ptr::null_mut();
        // SAFETY: `encoded_bytes` points to `encoded_size` live, initialized, unmutated bytes
        //         returned by `ccgen_imagejpeg_encode444`, and out-parameters are valid.
        let ret = unsafe {
            ccgen_zunejpeg_decode(
                encoded_bytes,
                encoded_size,
                Some(&mut decoded_w),
                Some(&mut decoded_h),
                Some(&mut decoded_bytes),
            )
        };
        assert_eq!(ret, OK);
        assert_eq!(decoded_w, W);
        assert_eq!(decoded_h, H);
        assert!(!decoded_bytes.is_null());

        // SAFETY: `(encoded_bytes, encoded_size)` was returned by `ccgen_imagejpeg_encode444`, is
        //         uniquely owned, and is freed once here.
        unsafe { ccgen_imagejpeg_free_buffer(encoded_bytes, encoded_size) };
        // SAFETY: `(decoded_bytes, decoded_w * decoded_h * 3)` was returned by
        //         `ccgen_zunejpeg_decode`, is uniquely owned, and is freed once here.
        unsafe { ccgen_imagejpeg_free_buffer(decoded_bytes, decoded_w * decoded_h * 3) };
    }

    #[test]
    fn ffi_rejects_overflowing_and_invalid_inputs() {
        let dummy: [u8; 4] = [0; 4];

        let invalid_webp = ccgen_WebPData {
            bytes: dummy.as_ptr(),
            size: dummy.len(),
        };
        // SAFETY: `dummy` points to `dummy.len()` valid bytes.
        let dec = unsafe { ccgen_WebPAnimDecoderNewInternal(Some(&invalid_webp), None, 0) };
        assert!(dec.is_null());

        let oversized_webp = ccgen_WebPData {
            bytes: dummy.as_ptr(),
            size: (isize::MAX as usize) + 1,
        };
        // SAFETY: `ccgen_WebPAnimDecoderNewInternal` rejects `size > isize::MAX` before reading.
        let dec = unsafe { ccgen_WebPAnimDecoderNewInternal(Some(&oversized_webp), None, 0) };
        assert!(dec.is_null());

        let mut out_bytes = std::ptr::null_mut();
        let mut out_size = 0;
        // SAFETY: `ccgen_imagejpeg_encode444` rejects overflowing dimensions before reading.
        let ret = unsafe {
            ccgen_imagejpeg_encode444(
                dummy.as_ptr(),
                usize::MAX,
                1,
                3,
                75,
                Some(&mut out_bytes),
                Some(&mut out_size),
            )
        };
        assert_eq!(ret, ERROR);

        let mut w = 0;
        let mut h = 0;
        // SAFETY: `dummy` points to `dummy.len()` valid bytes.
        let ret = unsafe {
            ccgen_zunejpeg_decode(
                dummy.as_ptr(),
                dummy.len(),
                Some(&mut w),
                Some(&mut h),
                Some(&mut out_bytes),
            )
        };
        assert_eq!(ret, ERROR);

        // SAFETY: `ccgen_zunejpeg_decode` rejects `encoded_size > isize::MAX` before reading.
        let ret = unsafe {
            ccgen_zunejpeg_decode(
                dummy.as_ptr(),
                (isize::MAX as usize) + 1,
                Some(&mut w),
                Some(&mut h),
                Some(&mut out_bytes),
            )
        };
        assert_eq!(ret, ERROR);
    }
}
