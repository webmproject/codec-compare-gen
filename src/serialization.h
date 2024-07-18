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

#ifndef SRC_SERIALIZATION_H_
#define SRC_SERIALIZATION_H_

#include <string>
#include <vector>

#include "src/base.h"

namespace codec_compare_gen {

// Removes trailing or tailing spaces.
std::string Trim(const std::string& str);

// Splits the input string into tokens separated by delimiter.
// Keeps escaped tokens as is. Example: "a,b",c gives two tokens.
std::vector<std::string> Split(const std::string& str, char delimiter);

// Escapes the quotes in the input string and adds leading and trailing quotes.
std::string Escape(const std::string& str);
// Removes leading and trailing quotes and replaces each \" by ".
StatusOr<std::string> Unescape(const std::string& escaped_str, bool quiet);

// Enum/string conversions.
std::string SubsamplingToString(Subsampling chroma_subsampling);
StatusOr<Subsampling> SubsamplingFromString(const std::string& str, bool quiet);

}  // namespace codec_compare_gen

#endif  // SRC_SERIALIZATION_H_
