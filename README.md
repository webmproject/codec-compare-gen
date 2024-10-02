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

## CMake build

The following instructions are used to build the library and the `ccgen` command
line tool.

Clone the codec-compare-gen repository. Then run from its root folder:

```sh
./deps.sh
cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++
cmake --build build
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
  --codec combination 444 5 \
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
`libgtest-dev` must be installed on the system. Run `deps.sh` if not done yet.

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## C++ style

Use the following to format the code:

```sh
clang-format -style=file -i src/*.cc src/*.h tests/*.cc tools/*.cc
```

## License

See the [Apache v2.0 license](LICENSE) file.
