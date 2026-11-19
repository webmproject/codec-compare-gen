include(FetchContent)
include(CcgenFetchContent)

# OVERRIDE_FIND_PACKAGE makes find_package(GIF) return this local giflib in
# dependencies such as libwebp2, like for libpng.
FetchContent_Declare(
  GIF
  GIT_REPOSITORY "https://git.code.sf.net/p/giflib/code"
  GIT_TAG 5.2.2
  GIT_PROGRESS ON
  UPDATE_COMMAND "" OVERRIDE_FIND_PACKAGE)

# giflib has no CMakeLists.txt so this only fetches the sources.
ccgen_fetchcontent_makeavailable(GIF)

if(NOT TARGET gif)
  add_library(gif STATIC
    "${gif_SOURCE_DIR}/dgif_lib.c"
    "${gif_SOURCE_DIR}/egif_lib.c"
    "${gif_SOURCE_DIR}/gifalloc.c"
    "${gif_SOURCE_DIR}/gif_err.c"
    "${gif_SOURCE_DIR}/gif_font.c"
    "${gif_SOURCE_DIR}/gif_hash.c"
    "${gif_SOURCE_DIR}/openbsd-reallocarray.c"
    "${gif_SOURCE_DIR}/quantize.c")

  target_include_directories(gif PUBLIC "$<BUILD_INTERFACE:${gif_SOURCE_DIR}>")
  add_library(GIF::GIF ALIAS gif)
endif()

set(GIF_INCLUDE_DIRS "${gif_SOURCE_DIR}" CACHE PATH "" FORCE)
set(GIF_INCLUDE_DIR "${gif_SOURCE_DIR}" CACHE PATH "" FORCE)
set(GIF_LIBRARIES gif CACHE STRING "" FORCE)
# libwebp2 checks that a program can be linked with ${GIF_LIBRARIES}, which is
# not possible before the gif target is built.
set(GIF_COMPILES TRUE CACHE BOOL "GIF compiles" FORCE)
