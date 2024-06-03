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

#ifndef SRC_WORKER_H_
#define SRC_WORKER_H_

#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

namespace codec_compare_gen {

template <typename WorkerContext, typename WorkerImpl>
class Worker {
 public:
  Worker(Worker&&) = default;
  virtual ~Worker() = default;

  // Inherit and implement the public functions below.
  virtual bool AssignTask(WorkerContext& context) = 0;
  // Run if AssignTask() returned true.
  virtual void DoTask() = 0;
  // Run after DoTask().
  virtual void EndTask(WorkerContext& context) {}
  // At most one worker at a time is in AssignTask() or EndTask().

 protected:
  const size_t worker_id_;  // Mostly for debugging.

 private:
  Worker(size_t worker_id, std::mutex& mutex, bool multithreaded,
         WorkerContext& context)
      : worker_id_(worker_id),
        multithreaded_(multithreaded),
        mutex_(mutex),
        context_(context) {}

  bool LockAndAssignTask() {
    mutex_.lock();
    const bool assign = AssignTask(context_);
    mutex_.unlock();
    return assign;
  }
  void LockAndEndTask() {
    mutex_.lock();
    EndTask(context_);
    mutex_.unlock();
  }

  void Start() {
    if (multithreaded_) {
      thread_ = std::thread(&Worker::Run, this);
    } else {
      Run();
    }
  }
  void Run() {
    while (LockAndAssignTask()) {
      DoTask();
      LockAndEndTask();
    }
  }
  void Finish() {
    if (multithreaded_) thread_.join();
  }

  const bool multithreaded_;  // Whether this worker runs in an extra thread or
                              // in the main thread.
  std::thread thread_;        // Used only if multithreaded_.
  std::mutex& mutex_;         // Reference to WorkerPool::mutex_.
  WorkerContext& context_;

  template <typename A, typename B>
  friend class WorkerPool;
};

template <typename WorkerContext, typename WorkerImpl>
class WorkerPool {
 public:
  explicit WorkerPool(size_t num_workers) : num_workers_(num_workers) {}

  void Run(WorkerContext& context) {
    std::vector<WorkerImpl> workers;
    workers.reserve(num_workers_);
    for (size_t i = 0; i < num_workers_; ++i) {
      const bool multithreaded = i + 1 < num_workers_;
      workers.push_back(WorkerImpl(i, mutex_, multithreaded, context));
      workers.back().Start();
    }
    for (WorkerImpl& worker : workers) {
      worker.Finish();
    }
  }

 private:
  const size_t num_workers_;
  std::mutex mutex_;
};

}  // namespace codec_compare_gen

#endif  // SRC_WORKER_H_
