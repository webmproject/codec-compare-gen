include(FetchContent)

FetchContent_Declare(
  dssim URL https://github.com/kornelski/dssim/archive/refs/tags/3.4.0.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP false)
FetchContent_MakeAvailable(dssim)

add_custom_target(
  dssim_binary
  COMMAND cargo build --release
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/_deps/dssim-src)
