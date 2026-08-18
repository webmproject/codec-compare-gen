include(FetchContent)
include(CcgenFetchContent)

FetchContent_Declare(
  libwebp2
  GIT_REPOSITORY "https://chromium.googlesource.com/codecs/libwebp2"
  GIT_TAG bb7941c582786ff4079e853b2b9bb6007bde6c82
  GIT_PROGRESS ON
  UPDATE_COMMAND "")

set(WP2_BUILD_EXAMPLES
    OFF
    CACHE INTERNAL "")
set(WP2_BUILD_EXTRAS
    OFF
    CACHE INTERNAL "")
set(WP2_BUILD_TESTS
    OFF
    CACHE INTERNAL "")

ccgen_fetchcontent_makeavailable(libwebp2)

target_include_directories(webp2
                           INTERFACE $<BUILD_INTERFACE:${libwebp2_SOURCE_DIR}>)

if(CCGEN_ENABLE_PNG)
  add_dependencies(webp2 png_static)
  if(TARGET imageio)
    add_dependencies(imageio png_static)
  endif()
endif()

if(CCGEN_ENABLE_JPEG)
  add_dependencies(webp2 mozjpeg)
  if(TARGET imageio)
    add_dependencies(imageio mozjpeg)
  endif()
endif()

if(CCGEN_ENABLE_WEBP)
  add_dependencies(webp2 webp)
  if(TARGET imageio)
    add_dependencies(imageio webp)
  endif()
endif()
