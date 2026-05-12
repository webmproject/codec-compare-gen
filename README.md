# Codec-Compare-Gen

Simple C++ framework to generate codec compression performance comparison data.
Codecs are used to compress and decompress images using their library APIs for
better encoding and decoding timing (to exclude file loading from the recorded
duration), compared to calling separately built codec binaries. It also
centralizes data set reading, to avoid metadata or sample handling differences.

The generated data can be displayed using
[Codec-Compare](https://github.com/webmproject/codec-compare).

## Introduction

Build `tools/ccgen.cc` and look at the description given by the `--help` flag.

The `libccgen` API entrypoint lies in `src/framework.h`.

## Build

The following instructions are used to build the library and the `ccgen` command
line tool.

### Requirements

```sh
# For libjxl
sudo apt install libhwy-dev
```

### Instructions

Clone the codec-compare-gen repository. Then run from its root folder:

```sh
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
```

### Generate JSON data

Example Unix command line:

```sh
mkdir -p output/encoded
build/ccgen \
  --codec webp 444 9 \
  --codec webp2 444 6 \
  --codec jpegxl 444 7 \
  --codec avif 444 6 \
  --lossless \
  --repeat 15 \
  --threads $(($(nproc) - 1)) \
  --progress_file "output/progress.csv" \
  --results_folder "output/" \
  --encoded_folder "output/encoded" \
  -- "tests/data"
```

- `tests/data` is used as input images.
- `output/progress.csv` will contain the metrics of each encoding/decoding (file
  size, timings, distortion). This is useful to be able to start the benchmark
  from where it left off in case it was halted.
- `output/` will contain one JSON file per codec configuration, aggregated over
  all repetitions to smooth the timings.
- `output/encoded` will contain the compressed image files.

## Tests

The following instructions are used to make sure the unit tests pass.
`libgtest-dev` must be installed on the system.

```sh
cmake -S . -B build \
  -DCCGEN_BUILD_TESTING=ON \
  -DCCGEN_ENABLE_AVIF=ON \
  -DCCGEN_ENABLE_HEIF=ON \
  -DCCGEN_ENABLE_JPEG=ON \
  -DCCGEN_ENABLE_JPEG2000=ON \
  -DCCGEN_ENABLE_JPEGXL=ON \
  -DCCGEN_ENABLE_WEBP=ON \
  -DCCGEN_ENABLE_WEBP2=ON \
  -DCCGEN_ENABLE_FFV1=ON \
  -DCCGEN_ENABLE_BASIS=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -j$(nproc)
```

## WASM/emscripten build

```sh
emcmake cmake -S . -B build_wasm \
  -DCCGEN_ENABLE_JPEGXL=OFF \
  -DCCGEN_ENABLE_WEBP2=ON \
  -DCCGEN_ENABLE_DSSIM=OFF \
  -DCCGEN_WASM=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF
emmake cmake --build build_wasm -j$(nproc)
```

### Typescript tests

```sh
emcmake cmake -S . -B build_wasm \
  -DCCGEN_BUILD_TESTING=ON -DCCGEN_ENABLE_JPEG=ON -DCCGEN_ENABLE_JPEGXL=OFF \
  -DCCGEN_ENABLE_WEBP=ON -DCCGEN_ENABLE_WEBP2=ON -DCCGEN_ENABLE_DSSIM=OFF \
  -DCCGEN_WASM=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
emmake cmake --build build_wasm -j$(nproc)
ctest --test-dir build --output-on-failure -j$(nproc)
```

## C++ style

Use the following to format the code:

```sh
clang-format -style=file -i src/* tests/*.cc tools/ wasm/*.cc
cmake-format -i CMakeLists.txt cmake/Modules/*
```

## License

See the [Apache v2.0 license](LICENSE) file.
