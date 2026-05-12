include(FetchContent)
include(CcgenFetchContent)

FetchContent_Declare(
  libavif
  GIT_REPOSITORY "https://github.com/AOMediaCodec/libavif.git"
  # GIT_TAG v1.4.1 does not contain the latest AVM tag.
  GIT_TAG 257b1e45f979491d905786d80691ba33a7597290
  GIT_PROGRESS ON
  GIT_SHALLOW OFF
  UPDATE_COMMAND "")

set(AVIF_BUILD_APPS
    OFF
    CACHE INTERNAL "")
set(AVIF_BUILD_EXAMPLES
    OFF
    CACHE INTERNAL "")
set(AVIF_BUILD_TESTS
    OFF
    CACHE INTERNAL "")
set(AVIF_CODEC_AOM
    LOCAL
    CACHE INTERNAL "")
set(AVIF_CODEC_DAV1D
    LOCAL
    CACHE INTERNAL "")
set(AVIF_CODEC_AVM
    LOCAL
    CACHE INTERNAL "")
set(AVIF_LIBYUV
    LOCAL
    CACHE INTERNAL "")
set(AVIF_LIBSHARPYUV
    LOCAL
    CACHE INTERNAL "")
set(AVIF_ENABLE_EXPERIMENTAL_MINI
    ON
    CACHE INTERNAL "")

include_directories("${CMAKE_BINARY_DIR}/flatbuffers/include")

ccgen_fetchcontent_makeavailable(libavif)

if(CCGEN_ENABLE_WEBP)
  # To avoid building libsharpyuv twice.
  add_dependencies(avif webp)
endif()
