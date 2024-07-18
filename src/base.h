// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_BASE_H_
#define SRC_BASE_H_

#include <cassert>
#include <cstddef>
#include <iostream>
#include <utility>

namespace codec_compare_gen {

//------------------------------------------------------------------------------
// Constants

enum class Status { kOk, kUnknownError };

enum class Codec {
  kWebp,
  kWebp2,
  kJpegXl,
  kAvif,
  kCombination,
  kJpegturbo,
  kJpegli,
  kJpegsimple,
  kJpegmoz
};

static constexpr int kQualityLossless = -1;  // Input setting.

enum class Subsampling {
  kDefault,  // Default setting depending on the quality and/or codec.
  k444,      // No subsampling.
  k420       // Chroma subsampling 4:2:0 (halved in both dimensions).
};

enum class DistortionMetric {
  kLibwebp2Psnr,
  kLibwebp2Ssim,
  kDssim,
  kLibjxlButteraugli,
  kLibjxlSsimulacra,
  kLibjxlSsimulacra2,
  kLibjxlP3norm
};
static constexpr size_t kNumDistortionMetrics =
    static_cast<size_t>(DistortionMetric::kLibjxlP3norm) + 1;
static constexpr const char* kDistortionMetricToStr[] = {
    "PSNR",       "SSIM",        "DSSIM", "Butteraugli",
    "SSimulacra", "SSimulacra2", "P3norm"};
static_assert(sizeof(kDistortionMetricToStr) /
                  sizeof(kDistortionMetricToStr[0]) ==
              kNumDistortionMetrics);
static constexpr float kNoDistortion = 99.f;  // Measured dB (for PSNR).

// Lenient threshold to avoid aborting the whole data generation just because of
// a few faulty data points.
static constexpr size_t kMaxNumFailures = 32;

//------------------------------------------------------------------------------
// Status management

template <typename T>
struct StatusOr {
  Status status;
  T value;
  StatusOr(StatusOr<T>&& status_or)
      : status(status_or.status), value(std::move(status_or.value)) {}
  StatusOr(Status status) : status(status), value() {
    if (status == Status::kOk) {
      std::cerr << "Error: Status::kOk returned as StatusOr" << std::endl;
      this->status = Status::kUnknownError;
      assert(false);
    }
  }
  StatusOr(T&& value) : status(Status::kOk), value(std::move(value)) {}
  StatusOr<T>& operator=(StatusOr<T>&& status_or) = default;
};

// Can be used as follows:
//   OK_OR_RETURN(FuncThatReturnsStatusOrType());
#define OK_OR_RETURN(STATUS)                                  \
  do {                                                        \
    const Status checked_status = (STATUS);                   \
    if (checked_status != Status::kOk) return checked_status; \
  } while (false)

// Hack for generating a unique local variable, needed by ASSIGN_OR_RETURN().
#define CONCAT_IMPL(A, B) A##B
#define CONCAT(A, B) CONCAT_IMPL(A, B)
// Can be used as follows:
//   ASSIGN_OR_RETURN(const Type a, FuncThatReturnsStatusOrType());
// clang-format off
#define ASSIGN_OR_RETURN(A, B) \
  auto CONCAT(s,__LINE__)=(B);OK_OR_RETURN(CONCAT(s,__LINE__).status);A=std::move(CONCAT(s,__LINE__).value)
// clang-format on

struct LogError {
  explicit LogError(bool quiet) : quiet(quiet) {
    // TODO(yguyon): Return an std::string error message instead of cerr now?
    if (!quiet) std::cerr << "Error: ";
  }
  ~LogError() {
    if (!quiet) std::cerr << std::endl;
  }
  template <typename T>
  LogError& operator<<(const T& message) {
    if (!quiet) std::cerr << message;
    return *this;
  }
  operator Status() const { return Status::kUnknownError; }
  template <typename T>
  operator StatusOr<T>() const {
    return Status::kUnknownError;
  }
  const bool quiet;
};

// Can be used as follows:
//   CHECK_OR_RETURN(should_be_1 == 1, cerr_is_disabled) << should_be_1;
#define CHECK_OR_RETURN(CONDITION, QUIET) \
  if (!(CONDITION))                       \
  return LogError(QUIET) << "(" << (__FILE__) << ":" << (__LINE__) << ") "

//------------------------------------------------------------------------------

}  // namespace codec_compare_gen

#endif  // SRC_BASE_H_
