include(FetchContent)
include(CcgenFetchContent)

FetchContent_Declare(
  ZLIB
  GIT_REPOSITORY "https://github.com/madler/zlib"
  GIT_TAG v1.3.2
  GIT_PROGRESS ON
  UPDATE_COMMAND "" OVERRIDE_FIND_PACKAGE SYSTEM)

set(ZLIB_BUILD_SHARED
    OFF
    CACHE INTERNAL "")
set(ZLIB_BUILD_STATIC
    ON
    CACHE INTERNAL "")
set(ZLIB_BUILD_TESTING
    OFF
    CACHE INTERNAL "")
ccgen_fetchcontent_makeavailable(ZLIB)

target_include_directories(zlibstatic
                           INTERFACE "$<BUILD_INTERFACE:${zlib_SOURCE_DIR}>")

add_library(ZLIB::ZLIB ALIAS zlibstatic)

set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR})
set(ZLIB_LIBRARIES zlibstatic)
find_package(ZLIB REQUIRED)
message(STATUS "ZLIB_INCLUDE_DIRS: ${ZLIB_INCLUDE_DIRS}")
message(STATUS "ZLIB_LIBRARIES: ${ZLIB_LIBRARIES}")
