#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
/**
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
        assert!(picture.memory_.is_null());
        assert!(picture.memory_argb_.is_null());

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
/// `picture.argb` must point to a valid array of size `picture.argb_stride * picture.height`.
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

        if config.lossless == 0 {
            // The Rust image crate does not support encoding lossy WebP.
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
        let width = picture.width as usize;
        let height = picture.height as usize;
        let stride = picture.argb_stride as usize; // In pixels, not bytes.
        let argb_size = stride.checked_mul(height).unwrap(); // Number of u32 elements.

        // image::ExtendedColorType::Bgra8 is not supported by image::codecs::webp::WebPEncoder.
        // Convert from ccgen_WebPPicture's little-endian 32-bit ARGB (8-bit BGRA) to 8-bit RGBA.
        // SAFETY: `picture.argb` is checked against null above. The caller ensures
        //         `picture.argb` points to a buffer containing `argb_size` elements.
        let argb32 = unsafe { std::slice::from_raw_parts(picture.argb as *const u32, argb_size) };
        let mut rgba8 =
            Vec::with_capacity(width.checked_mul(height).unwrap().checked_mul(4).unwrap());
        for row in 0..(height as usize) {
            let row_start = row * stride;
            for col in 0..(width as usize) {
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
/// `webp_data.bytes` must point to a valid array of `webp_data.size` bytes.
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
        if webp_data.bytes.is_null() || webp_data.size == 0 {
            return std::ptr::null_mut();
        }
        // SAFETY: `webp_data.bytes` and `webp_data.size` are checked above. The caller ensures
        //         `webp_data.bytes` points to a valid array of `webp_data.size` bytes.
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
        let num_pixels = (width as usize).checked_mul(height as usize).unwrap();
        let buffer_len = num_pixels.checked_mul(4).unwrap();
        let expected_buffer_len = num_pixels * if decoder.has_alpha() { 4 } else { 3 };
        // dec.buffer will contain RGBA samples in this order.
        if dec.buffer.is_empty() {
            if dec.buffer.try_reserve_exact(buffer_len).is_err() {
                return ERROR;
            };
            dec.buffer.resize(buffer_len, 0); // Initial values do not matter.
        } else {
            assert_eq!(dec.buffer.len(), buffer_len);
        }

        let delay_ms = if decoder.is_animated() {
            if dec.next_frame_index >= decoder.num_frames() {
                return ERROR;
            }
            // image_webp::WebPDecoder::read_frame() will fill the given buffer with RGBA samples
            // if image_webp::WebPDecoder::has_alpha() is true, and RGB samples otherwise.
            match decoder.read_frame(&mut dec.buffer[..expected_buffer_len]) {
                Ok(delay_ms) => delay_ms,
                Err(_) => return 0,
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

        dec.next_frame_index += 1;
        dec.next_frame_timestamp_ms = dec.next_frame_timestamp_ms.saturating_add(delay_ms);
        *buf = dec.buffer.as_mut_ptr();
        *timestamp = std::ffi::c_int::try_from(dec.next_frame_timestamp_ms).unwrap();
        OK
    } else {
        ERROR
    }
}

/// Deletes a `ccgen_WebPAnimDecoder`.
///
/// # Safety
/// Unless none, `dec` must point to a valid `ccgen_WebPAnimDecoder` allocated by
/// `ccgen_WebPAnimDecoderNewInternal()`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ccgen_WebPAnimDecoderDelete(dec: *mut ccgen_WebPAnimDecoder) {
    if !dec.is_null() {
        // SAFETY: `dec` is checked against null above. The caller ensures it points to a valid
        //         `ccgen_WebPAnimDecoder` allocated by `Box::into_raw()`.
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
/// `rgb_pixels` must point to a valid array of size `stride * height`.
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
        if stride != width.checked_mul(3).unwrap() {
            return ERROR;
        }
        let slice_len = stride.checked_mul(height).unwrap();
        // SAFETY: rgb_pixels is checked against null above. The caller ensures
        //         it points to a buffer with `stride * height` bytes.
        let slice = unsafe { std::slice::from_raw_parts(rgb_pixels, slice_len) };

        let mut out_vec = Vec::new();
        let mut encoder =
            image::codecs::jpeg::JpegEncoder::new_with_quality(&mut out_vec, quality as u8);
        match encoder.encode(
            &slice,
            width as u32,
            height as u32,
            image::ExtendedColorType::Rgb8,
        ) {
            Ok(()) => {
                let mut boxed_slice = out_vec.into_boxed_slice();
                *output_size = boxed_slice.len();
                *output_bytes = boxed_slice.as_mut_ptr();
                std::mem::forget(boxed_slice);
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
/// `encoded_bytes` must point to a valid slice of size `encoded_size`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ccgen_zunejpeg_decode(
    encoded_bytes: *const u8,
    encoded_size: usize,
    width: Option<&mut usize>,
    height: Option<&mut usize>,
    output_bytes: Option<&mut *mut u8>,
) -> std::ffi::c_int {
    if let (Some(width), Some(height), Some(output_bytes)) = (width, height, output_bytes) {
        if encoded_bytes.is_null() || encoded_size == 0 {
            return ERROR;
        }
        // SAFETY: encoded_bytes is checked against null above. The caller ensures
        //         it points to a buffer with `encoded_size` bytes.
        let slice = unsafe { std::slice::from_raw_parts(encoded_bytes, encoded_size) };
        let cursor = zune_core::bytestream::ZCursor::new(slice);
        let options = zune_core::options::DecoderOptions::default()
            .jpeg_set_out_colorspace(zune_core::colorspace::ColorSpace::RGB);
        let mut decoder = zune_jpeg::JpegDecoder::new_with_options(cursor, options);
        match decoder.decode() {
            Ok(pixels) => {
                if let Some(info) = decoder.info() {
                    *width = info.width as usize;
                    *height = info.height as usize;
                    let mut boxed_slice = pixels.into_boxed_slice();
                    *output_bytes = boxed_slice.as_mut_ptr();
                    std::mem::forget(boxed_slice);
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
/// `buffer` must point to a valid slice of size `size` allocated by `into_boxed_slice()`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ccgen_imagejpeg_free_buffer(buffer: *mut u8, size: usize) {
    if !buffer.is_null() {
        // SAFETY: buffer is checked against null above. The caller ensures it points to a valid
        //         boxed slice of size `size`.
        let _ = unsafe { Box::from_raw(std::slice::from_raw_parts_mut(buffer, size)) };
    }
}
