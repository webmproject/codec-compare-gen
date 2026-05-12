include(FetchContent)
include(CcgenFetchContent)

FetchContent_Declare(
  libbasisu
  GIT_REPOSITORY "https://github.com/BinomialLLC/basis_universal.git"
  GIT_TAG v2_1_0
  GIT_PROGRESS ON
  GIT_SHALLOW ON
  UPDATE_COMMAND "")

set(BASISU_SSE
    ON
    CACHE INTERNAL "")

ccgen_fetchcontent_makeavailable(libbasisu)

target_include_directories(basisu_encoder
                           INTERFACE $<BUILD_INTERFACE:${libbasisu_SOURCE_DIR}>)
