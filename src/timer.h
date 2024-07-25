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

#ifndef SRC_TIMER_H_
#define SRC_TIMER_H_

#include <chrono>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>

namespace codec_compare_gen {

class Timer {
 public:
  static std::string SecondsToString(double seconds) {
    std::stringstream ss;
    const double hours = seconds / 3600;
    if (hours > 1) {
      ss << std::setprecision(2) << hours << " hours";
    } else {
      const double minutes = seconds / 60;
      if (minutes > 1) {
        ss << std::setprecision(2) << minutes << " minutes";
      } else {
        ss << std::fixed << std::setprecision(3) << seconds << " seconds";
      }
    }
    return ss.str();
  }

  Timer() : start_(std::chrono::high_resolution_clock::now()) {}
  double seconds() const {
    return std::chrono::duration<double>(
               std::chrono::high_resolution_clock::now() - start_)
        .count();
  }

 private:
  const std::chrono::high_resolution_clock::time_point start_;
};

#endif  // SRC_TIMER_H_

}  // namespace codec_compare_gen
