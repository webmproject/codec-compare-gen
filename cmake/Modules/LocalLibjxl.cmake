include(FetchContent)
include(CcgenFetchContent)

FetchContent_Declare(
  libjxl
  GIT_REPOSITORY "https://github.com/libjxl/libjxl.git"
  GIT_TAG v0.11.2
  GIT_PROGRESS ON
  GIT_SHALLOW ON
  UPDATE_COMMAND "")

set(JPEGXL_ENABLE_BENCHMARK
    OFF
    CACHE INTERNAL "")
set(JPEGXL_ENABLE_DOXYGEN
    OFF
    CACHE INTERNAL "")
set(JPEGXL_ENABLE_EXAMPLES
    OFF
    CACHE INTERNAL "")
set(JPEGXL_ENABLE_JNI
    OFF
    CACHE INTERNAL "")
set(JPEGXL_ENABLE_MANPAGES
    OFF
    CACHE INTERNAL "")
set(JPEGXL_ENABLE_OPENEXR
    OFF
    CACHE INTERNAL "")
set(JPEGXL_ENABLE_SJPEG
    OFF
    CACHE INTERNAL "")
set(JPEGXL_ENABLE_TRANSCODE_JPEG
    OFF
    CACHE INTERNAL "")
set(JPEGXL_ENABLE_WASM_THREADS
    OFF
    CACHE INTERNAL "")
set(JPEGXL_FORCE_SYSTEM_BROTLI
    OFF
    CACHE INTERNAL "")
set(JPEGXL_FORCE_SYSTEM_HWY
    OFF
    CACHE INTERNAL "")

# jpegli was still part of libjxl in v0.11.2 but will be removed. See
# https://github.com/libjxl/libjxl/pull/4657.
set(JPEGXL_ENABLE_JPEGLI
    ON
    CACHE INTERNAL "")
set(JPEGXL_ENABLE_JPEGLI_LIBJPEG
    OFF
    CACHE INTERNAL "")

# JPEGXL_ENABLE_DEVTOOLS=ON for Butteraugli and SSIMULACRA2 metric binaries. See
# https://github.com/cloudinary/ssimulacra2/blob/d2be72505ddc5c92aeb30f4a7f3ab53db45b314b/build_ssimulacra_from_libjxl_repo
set(JPEGXL_ENABLE_DEVTOOLS
    ON
    CACHE INTERNAL "")
# JPEGXL_ENABLE_DEVTOOLS=ON somehow requires JPEGXL_ENABLE_TOOLS=ON.
set(JPEGXL_ENABLE_TOOLS
    ON
    CACHE INTERNAL "")

ccgen_fetchcontent_makeavailable(libjxl)

target_include_directories(jxl
                           INTERFACE $<BUILD_INTERFACE:${libjxl_SOURCE_DIR}>)
