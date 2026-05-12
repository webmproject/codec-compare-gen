include(FetchContent)
include(CcgenFetchContent)

FetchContent_Declare(
  libwebp2
  GIT_REPOSITORY "https://chromium.googlesource.com/codecs/libwebp2"
  GIT_TAG 142784b3090acdf6d156dd81416f1a6b18fdbdf1
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
endif()

if(CCGEN_ENABLE_JPEG)
  add_dependencies(webp2 mozjpeg)
endif()

if(CCGEN_ENABLE_WEBP)
  add_dependencies(webp2 webp)
endif()
