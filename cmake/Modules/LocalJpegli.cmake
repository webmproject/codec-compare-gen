include(FetchContent)
include(CcgenFetchContent)

set(JPEGLI_GIT_TAG 031a0077f5799a6041004267fc12b956c1f52a20)

FetchContent_Declare(
  jpegli
  GIT_REPOSITORY "https://github.com/google/jpegli.git"
  GIT_TAG ${JPEGLI_GIT_TAG}
  GIT_PROGRESS ON
  GIT_SHALLOW ON
  UPDATE_COMMAND "")

set(JPEGLI_ENABLE_TOOLS
    OFF
    CACHE INTERNAL "")
set(JPEGLI_ENABLE_JPEGLI_LIBJPEG
    OFF
    CACHE INTERNAL "")
set(JPEGLI_ENABLE_DOXYGEN
    OFF
    CACHE INTERNAL "")
set(JPEGLI_ENABLE_MANPAGES
    OFF
    CACHE INTERNAL "")
set(JPEGLI_ENABLE_BENCHMARK
    OFF
    CACHE INTERNAL "")
set(JPEGLI_BUNDLE_LIBPNG
    OFF
    CACHE INTERNAL "")
set(JPEGLI_ENABLE_JNI
    OFF
    CACHE INTERNAL "")
set(JPEGLI_ENABLE_SJPEG
    OFF
    CACHE INTERNAL "")
set(JPEGLI_ENABLE_OPENEXR
    OFF
    CACHE INTERNAL "")

# Assume libjxl is built. libjxl already defines HWY targets. Reuse them to
# avoid CMake conflicts.
set(JPEGLI_FORCE_SYSTEM_HWY
    ON
    CACHE INTERNAL "")
set(HWY_FOUND
    ON
    CACHE INTERNAL "")
set(HWY_LIBRARY
    hwy
    CACHE INTERNAL "")
set(HWY_INCLUDE_DIR
    "${libjxl_SOURCE_DIR}/third_party/highway"
    CACHE INTERNAL "")

# Assume libjxl is built. libjxl already defines a "tool_version_git" target.
# Hardcode the version to avoid CMake conflicts.
set(JPEGLI_VERSION
    "${JPEGLI_GIT_TAG}"
    CACHE INTERNAL "")

ccgen_fetchcontent_makeavailable(jpegli)

target_include_directories(jpegli-static
                           INTERFACE $<BUILD_INTERFACE:${jpegli_SOURCE_DIR}>)
