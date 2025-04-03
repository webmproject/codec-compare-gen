# Changelog

## v0.5.4

- Add JPEG 2000 support through OpenJPEG (4:4:4 only).

## v0.5.3

- Fix jpegturbo, jpegli, and jpegmoz reproducing command lines in output JSON.
- Add subsampling to the codec version field values in output JSON.

## v0.5.2

- Bump the version of libwebp2 in deps.sh.
- Bump the version of libavif in deps.sh.

## v0.5.1

- Fix libavif-AVM dependency build commands.

## v0.5.0

- Update libavif, libwebp, libwebp2, and libjxl in deps.sh.
- Simplify AVIF variants to:
  - AVIF-AV1 regular ('meta', lossy YUV, lossless 8-bit RGB)
  - AVIF-AV1 experimental ('mini', lossy YUV, lossless 10-bit YCgCo-Re)
  - AVIF-AVM experimental ('mini', lossy YUV, lossless 10-bit YCgCo-Re)

## v0.4.3

- For the codecs that do not support 16-bit samples natively, encode 16-bit
  images as twice as large assets with high significant bits on the left side
  and low significant bits on the right side of the encoded 8-bit image, instead
  of alternating high and low significant bits as 8-bit samples per row.

## v0.4.2

- Keep only parent folder instead of full absolute image paths in JSON output.

## v0.4.1

- Bump the version of libwebp2 in deps.sh.
- Support encoding 16-bit images with JPEG XL. Encode 16-bit images as twice as
  wide 8-bit images with other codecs.

## v0.4.0

- Bump the version of libwebp2 in deps.sh.
- Support reading 16-bit images.

## v0.3.6

- Disable thread-unsafe CCSO tool in AVM.

## v0.3.5

- Add AVM support for AVIF.

## v0.3.4

- Add "HEIF low-overhead image file format" support for AVIF.

## v0.3.3

- Bump the version of libavif in deps.sh.
- Use AVIF_ENABLE_EXPERIMENTAL_YCGCO_R=ON in deps.sh and use
  AVIF_MATRIX_COEFFICIENTS_YCGCO_RE in codec_avif for lossless encoding.

## v0.3.2

- Fix ccgen --qualities min:max.

## v0.3.1

- Bump the version of libavif in deps.sh.
- Bump the version of libjxl in deps.sh.

## v0.3.0

- Add lossy animation support.
- Bump the version of libwebp2 in deps.sh.

## v0.2.2

- Bump the version of libwebp2 in deps.sh.

## v0.2.1

- Merge identical consecutive frames when reading animations.
- Warn about and discard 0-second frames when reading animations.
- Bump the version of libwebp2 in deps.sh.

## v0.2.0

- Add lossless animation support.
- Replace the libwebp2 wrapper in codec_webp by libwebp API calls to save a
  buffer copy.
- Use SharpYUV with the WebP codec.
- Bump the version of libwebp2 in deps.sh.

## v0.1.5

- Bump the versions of the dependencies in deps.sh.
- Allow values of `DistortionMetric::kLibjxlButteraugli` greater than 99.

## v0.1.4

- Make `--progress_file` and `--results_folder` optional in `ccgen.cc`.
- Print single result summary in `ccgen.cc`.

## v0.1.3

- Swap the order of the effort and chroma subsampling parameters everywhere.

## v0.1.2

- Add 4:2:0 chroma subsampling support for codecs that also support lossy 4:4:4.

## v0.1.1

- Add JPEG codec libraries jpegli, jpegturbo, mozjpeg and sjpeg.

## v0.1.0

- Initial release of the Codec-Compare-Gen library and tools.
