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

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "src/base.h"
#include "src/serialization.h"

namespace codec_compare_gen {
namespace {

TEST(SerializationTest, Trim) {
  EXPECT_EQ(Trim(""), "");
  EXPECT_EQ(Trim("a"), "a");
  EXPECT_EQ(Trim(" a"), "a");
  EXPECT_EQ(Trim("a "), "a");
  EXPECT_EQ(Trim(" a "), "a");
  EXPECT_EQ(Trim(" a b "), "a b");
}

TEST(SerializationTest, Split) {
  EXPECT_EQ(Split("", ','), std::vector<std::string>{""});
  EXPECT_EQ(Split("a", ','), std::vector<std::string>{"a"});
  EXPECT_EQ(Split("a, b", ','), std::vector<std::string>({"a", "b"}));
  EXPECT_EQ(Split("\"a, b\", c", ','),
            std::vector<std::string>({"\"a, b\"", "c"}));
}

TEST(SerializationTest, Escape) {
  EXPECT_EQ(Escape(""), "\"\"");
  EXPECT_EQ(Escape("a"), "\"a\"");
  EXPECT_EQ(Escape("\"a\""), "\"\\\"a\\\"\"");
}

TEST(SerializationTest, Unescape) {
  constexpr bool kQuiet = true;
  EXPECT_EQ(Unescape("", kQuiet).status, Status::kUnknownError);
  EXPECT_EQ(Unescape("\"", kQuiet).status, Status::kUnknownError);
  EXPECT_EQ(Unescape("\"a", kQuiet).status, Status::kUnknownError);
  EXPECT_EQ(Unescape("a\"", kQuiet).status, Status::kUnknownError);
  EXPECT_EQ(Unescape(" \"\"", kQuiet).status, Status::kUnknownError);
  EXPECT_EQ(Unescape("\"\" ", kQuiet).status, Status::kUnknownError);
  EXPECT_EQ(Unescape("\"\\\"", kQuiet).status, Status::kUnknownError);
  EXPECT_EQ(Unescape("\\\"\"", kQuiet).status, Status::kUnknownError);
  EXPECT_EQ(Unescape("\"\"", kQuiet).status, Status::kOk);
  EXPECT_EQ(Unescape("\"\"", kQuiet).value, "");
  EXPECT_EQ(Unescape("\" \"", kQuiet).value, " ");
  EXPECT_EQ(Unescape("\" a \"", kQuiet).value, " a ");
}

}  // namespace
}  // namespace codec_compare_gen
