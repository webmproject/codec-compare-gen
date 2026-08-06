include(FetchContent)
include(CcgenFetchContent)

FetchContent_Declare(
  libsjpeg
  GIT_REPOSITORY "https://github.com/webmproject/sjpeg.git"
  GIT_TAG 1afdc5508ef9c3d2d932e82f815b8dbbab4e6ae2
  GIT_PROGRESS ON
  GIT_SHALLOW OFF
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
