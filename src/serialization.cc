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

#include "src/serialization.h"

#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

#include "src/base.h"

namespace codec_compare_gen {

std::string Trim(const std::string& str) {
  for (size_t i = 0; i < str.size(); ++i) {
    if (std::isspace(str[i])) continue;
    for (size_t n = str.size(); n > i; --n) {
      if (std::isspace(str[n - 1])) continue;
      return str.substr(i, n - i);
    }
  }
  return "";
}

std::vector<std::string> Split(const std::string& str, char delimiter) {
  std::vector<std::string> tokens(1);
  bool is_escaped = false;
  bool in_literal_string = false;
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == delimiter && !in_literal_string) {
      tokens.push_back("");
      continue;
    }
    if (str[i] == '"' && !is_escaped) {
      in_literal_string = !in_literal_string;
    }
    is_escaped = (!is_escaped && str[i] == '\\');
    tokens.back().push_back(str[i]);
  }
  for (std::string& token : tokens) token = Trim(token);
  return tokens;
}

std::string Escape(const std::string& str) {
  std::string escaped_str("\"");
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '"') {
      escaped_str.push_back('\\');
    }
    escaped_str.push_back(str[i]);
  }
  escaped_str.push_back('\"');
  return escaped_str;
}

StatusOr<std::string> Unescape(const std::string& escaped_str, bool quiet) {
  CHECK_OR_RETURN(escaped_str.size() >= 2 && escaped_str.front() == '"' &&
                      escaped_str.back() == '"' &&
                      escaped_str[escaped_str.size() - 2] != '\\',
                  quiet)
      << escaped_str << " is not properly escaped";
  std::string str;
  for (size_t i = 1; i + 1 < escaped_str.size(); ++i) {
    if (escaped_str[i] == '\\' && escaped_str[i + 1] == '"') {
      str.push_back('"');
      ++i;
    } else {
      str.push_back(escaped_str[i]);
    }
  }
  return str;
}

}  // namespace codec_compare_gen
