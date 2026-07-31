include(FetchContent)
include(CcgenFetchContent)

FetchContent_Declare(
  libheif
  GIT_REPOSITORY "https://github.com/strukturag/libheif.git"
  GIT_TAG v1.23.1
  GIT_PROGRESS ON
  GIT_SHALLOW ON
  UPDATE_COMMAND "")

set(ENABLE_PLUGIN_LOADING
    OFF
    CACHE INTERNAL "")
set(WITH_AOM_DECODER
    OFF
    CACHE INTERNAL "")
set(WITH_AOM_ENCODER
    ON
    CACHE INTERNAL "")
set(WITH_DAV1D
    ON
    CACHE INTERNAL "")
set(WITH_LIBSHARPYUV
    OFF
    CACHE INTERNAL "")
set(ENABLE_MULTITHREADING_SUPPORT
    OFF
    CACHE INTERNAL "")
set(ENABLE_PARALLEL_TILE_DECODING
    OFF
    CACHE INTERNAL "")

# Reuse the libaom and dav1d dependencies from libavif.
set(AOM_INCLUDE_DIR
    "${CMAKE_BINARY_DIR}/_deps/libaom-src/"
    CACHE INTERNAL "")
set(AOM_LIBRARY
    "${CMAKE_BINARY_DIR}/_deps/libaom-build/libaom.a"
    CACHE INTERNAL "")
set(DAV1D_INCLUDE_DIR
    "${CMAKE_BINARY_DIR}/_deps/dav1d-install/include/"
    CACHE INTERNAL "")
set(DAV1D_LIBRARY
    "${CMAKE_BINARY_DIR}/_deps/dav1d-install/lib/libdav1d.a"
    CACHE INTERNAL "")

ccgen_fetchcontent_makeavailable(libheif)

target_include_directories(
  heif INTERFACE $<BUILD_INTERFACE:${libheif_SOURCE_DIR}>/libheif/api)
target_include_directories(heif
                           INTERFACE $<BUILD_INTERFACE:${libheif_BINARY_DIR}>)

add_dependencies(heif aom dav1d)
