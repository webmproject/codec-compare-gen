# Changelog

## v0.2.0

- Add animation support.
- Replace the libwebp2 wrapper in codec_webp by libwebp API calls to save a
  buffer copy.
- Use SharpYUV with the WebP codec.

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
