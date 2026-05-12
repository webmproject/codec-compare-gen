include(FetchContent)
include(CcgenFetchContent)

# Use the same FetchContent_Declare() args as libavif's libsharpyuv dependency.
FetchContent_Declare(
  libwebp
  GIT_REPOSITORY "https://chromium.googlesource.com/webm/libwebp"
  GIT_TAG v1.6.0
  GIT_PROGRESS ON
  GIT_SHALLOW ON
  UPDATE_COMMAND "")

set(WEBP_BUILD_ANIM_UTILS
    OFF
    CACHE INTERNAL "")
set(WEBP_BUILD_CWEBP
    OFF
    CACHE INTERNAL "")
set(WEBP_BUILD_DWEBP
    OFF
    CACHE INTERNAL "")
set(WEBP_BUILD_EXTRAS
    OFF
    CACHE INTERNAL "")
set(WEBP_BUILD_FUZZTEST
    OFF
    CACHE INTERNAL "")
set(WEBP_BUILD_GIF2WEBP
    OFF
    CACHE INTERNAL "")
set(WEBP_BUILD_IMG2WEBP
    OFF
    CACHE INTERNAL "")
set(WEBP_BUILD_VWEBP
    OFF
    CACHE INTERNAL "")
set(WEBP_BUILD_WEBPINFO
    OFF
    CACHE INTERNAL "")
set(WEBP_BUILD_WEBPMUX
    OFF
    CACHE INTERNAL "")

set(WEBP_BUILD_LIBWEBPMUX
    ON
    CACHE INTERNAL "")

ccgen_fetchcontent_makeavailable(libwebp)

if(CCGEN_WASM)
  add_library(webpmux ALIAS libwebpmux)
endif()

target_include_directories(
  webp INTERFACE $<BUILD_INTERFACE:${libwebp_SOURCE_DIR}>
                 $<BUILD_INTERFACE:${libwebp_SOURCE_DIR}/src>)

set(WEBP_INCLUDE_DIRS ${libwebp_SOURCE_DIR}/src)
set(WEBP_LIBRARIES webpmux webpdemux webp)

if(CCGEN_ENABLE_JPEG)
  add_dependencies(webp mozjpeg)
endif()

# Skip libwebp2's find_package(WebP).
FetchContent_Declare(WebP DOWNLOAD_COMMAND "" OVERRIDE_FIND_PACKAGE SYSTEM)
