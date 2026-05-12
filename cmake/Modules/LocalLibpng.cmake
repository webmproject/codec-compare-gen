include(FetchContent)
include(CcgenFetchContent)

FetchContent_Declare(
  PNG
  GIT_REPOSITORY "https://github.com/pnggroup/libpng"
  GIT_TAG v1.6.58
  GIT_PROGRESS ON
  UPDATE_COMMAND "" OVERRIDE_FIND_PACKAGE SYSTEM)

set(PNG_SHARED
    OFF
    CACHE INTERNAL "")
set(ZLIB_USE_STATIC_LIBS
    ON
    CACHE INTERNAL "")

ccgen_fetchcontent_makeavailable(PNG)

target_include_directories(png_static
                           INTERFACE "$<BUILD_INTERFACE:${png_SOURCE_DIR}>")
target_link_libraries(png_static PRIVATE zlibstatic)
target_link_libraries(png_static PRIVATE ZLIB::ZLIB)

add_library(PNG::PNG ALIAS png_static)

add_dependencies(png_static zlibstatic)

set(PNG_INCLUDE_DIRS ${PNG_SOURCE_DIR})
set(PNG_LIBRARIES png_static)
