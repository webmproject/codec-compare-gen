# C-to-Rust bindings from libwebp API to image-webp crate

* `./webp` is a copy of `libwebp/src/webp` with the following changes:

  * "_rs" suffix added to each file name.
  * "ccgen_" prefix added to each symbol.

* `lib.rs` contains unsafe bindings from the C headers in `./webp` to the
  image-webp crate implementation.
* `Cargo.toml` contains the project and dependency definition.

## Usage

1. Run `cargo build`.
2. Replace the libwebp binary with the generated binary.
3. Include the C headers from `./webp` instead of the `libwebp/src/webp` ones.
4. Add the "ccgen_" prefix to each libwebp API symbol used.

## Considered alternatives

* https://github.com/google/safe-bindings/tree/main/pixel_bridge: does not wrap
  the image-webp crate (lacks background color and frame count access).
* crubit: requires a nightly version of Rust.
* bindgen: copy-pasting the headers from libwebp was as easy as generating them.
