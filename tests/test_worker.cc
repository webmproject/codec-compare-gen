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

#include <chrono>

#include "gtest/gtest.h"
#include "src/worker.h"

namespace codec_compare_gen {
namespace {

void Wait() {
  std::chrono::high_resolution_clock::time_point start =
      std::chrono::high_resolution_clock::now();
  while (std::chrono::duration<double>(
             std::chrono::high_resolution_clock::now() - start)
             .count() < /*seconds=*/0.1) {
  }
}

struct WorkerContext {
  int to_do;
  int done;
};

class TestWorker : public Worker<WorkerContext, TestWorker> {
  using Worker<WorkerContext, TestWorker>::Worker;
  bool AssignTask(WorkerContext& context) override {
    if (context.to_do == 0) return false;
    --context.to_do;
    return true;
  }
  void DoTask() override { Wait(); }
  void EndTask(WorkerContext& context) override { ++context.done; }
};

TEST(WorkerTest, PoolOf0) {
  WorkerContext context = {/*to_do=*/2, /*done=*/0};
  WorkerPool<WorkerContext, TestWorker> pool(/*num_workers=*/0);
  pool.Run(context);
  EXPECT_EQ(context.to_do, 2);
  EXPECT_EQ(context.done, 0);
}

TEST(WorkerTest, PoolOf1) {
  WorkerContext context = {/*to_do=*/2, /*done=*/0};
  WorkerPool<WorkerContext, TestWorker> pool(/*num_workers=*/1);
  pool.Run(context);
  EXPECT_EQ(context.to_do, 0);
  EXPECT_EQ(context.done, 2);
}

TEST(WorkerTest, PoolOf2) {
  WorkerContext context = {/*to_do=*/2, /*done=*/0};
  WorkerPool<WorkerContext, TestWorker> pool(/*num_workers=*/2);
  pool.Run(context);
  EXPECT_EQ(context.to_do, 0);
  EXPECT_EQ(context.done, 2);
}

TEST(WorkerTest, PoolOf10) {
  WorkerContext context = {/*to_do=*/2, /*done=*/0};
  WorkerPool<WorkerContext, TestWorker> pool(/*num_workers=*/10);
  pool.Run(context);
  EXPECT_EQ(context.to_do, 0);
  EXPECT_EQ(context.done, 2);
}

}  // namespace
}  // namespace codec_compare_gen
