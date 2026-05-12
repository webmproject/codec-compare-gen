include(FetchContent)
include(CcgenFetchContent)

FetchContent_Declare(
  libsjpeg
  GIT_REPOSITORY "https://github.com/webmproject/sjpeg.git"
  GIT_TAG 46da5aec5fce05faabf1facf0066e36e6b1c4dff
  GIT_PROGRESS ON
  GIT_SHALLOW ON
  UPDATE_COMMAND "")

set(SJPEG_BUILD_EXAMPLES
    OFF
    CACHE INTERNAL "")

ccgen_fetchcontent_makeavailable(libsjpeg)

target_include_directories(sjpeg
                           INTERFACE $<BUILD_INTERFACE:${libsjpeg_SOURCE_DIR}>)

if(CCGEN_ENABLE_PNG)
  add_dependencies(sjpeg png_static)
endif()

if(CCGEN_ENABLE_JPEG)
  add_dependencies(sjpeg mozjpeg)
endif()
