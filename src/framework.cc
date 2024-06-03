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

#include "src/framework.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/codec.h"
#include "src/result_json.h"
#include "src/task.h"
#include "src/worker.h"

using seconds = std::chrono::duration<double>;
using chrono = std::chrono::high_resolution_clock;

namespace codec_compare_gen {
namespace {

// Shared among all TaskWorkers. Guarded by a mutex in WorkerPool.
struct WorkerContext {
  Status status = Status::kOk;  // kOk or first encountered error.
  std::vector<TaskOutput> completed_tasks;
  std::vector<TaskInput> remaining_tasks;
  bool load_encoded_from_disk = false;
  std::unordered_set<std::string> written_files;
  std::string completed_tasks_file_path;
  std::ofstream completed_tasks_file;
  std::string metric_binary_folder_path;
  size_t num_tasks = 0;
  size_t num_failures = 0;

  bool quiet = true;
  size_t num_completed_tasks_since_start = 0;
  chrono::time_point start_time = chrono::now();
  chrono::time_point last_progress_display_time = chrono::now();
};

class TaskWorker : public Worker<WorkerContext, TaskWorker> {
 public:
  using Worker<WorkerContext, TaskWorker>::Worker;

 private:
  bool AssignTask(WorkerContext& context) override {
    if (context.remaining_tasks.empty()) return false;
    current_task_input_ = context.remaining_tasks.back();
    metric_binary_folder_path_ = context.metric_binary_folder_path;
    if (context.load_encoded_from_disk) {
      encode_mode_ = EncodeMode::kLoadFromDisk;
    } else {
      // Only save to disk the first occurrence of the same file to avoid any
      // disk access concurrency issue.
      encode_mode_ =
          !current_task_input_.encoded_path.empty() &&
                  context.written_files.insert(current_task_input_.encoded_path)
                      .second
              ? EncodeMode::kEncodeAndSaveToDisk
              : EncodeMode::kEncode;
    }
    context.remaining_tasks.pop_back();
    quiet_ = context.quiet;
    return true;
  }

  void DoTask() override {
    current_task_output_ =
        EncodeDecode(current_task_input_, metric_binary_folder_path_,
                     worker_id_, encode_mode_, quiet_);
    if (current_task_output_.status != Status::kOk) return;
    serialized_current_task_output_ = current_task_output_.value.Serialize();
  }

  void EndTask(WorkerContext& context) override {
    if (current_task_output_.status == Status::kOk) {
      if (!context.completed_tasks_file_path.empty()) {
        context.completed_tasks_file << serialized_current_task_output_
                                     << std::endl;
      }
      context.completed_tasks.push_back(current_task_output_.value);
      ++context.num_completed_tasks_since_start;
    } else {
      if (context.status == Status::kOk) {
        context.status = current_task_output_.status;
      }
      --context.num_tasks;
      ++context.num_failures;
      if (context.num_failures > kMaxNumFailures) {
        // Drain remaining tasks to exit quickly.
        context.remaining_tasks.clear();
      }
    }
    serialized_current_task_output_.clear();

    if (!quiet_) {
      const double duration_since_last_progress_display =
          seconds(chrono::now() - context.last_progress_display_time).count();
      if (duration_since_last_progress_display > 30) {
        context.last_progress_display_time = chrono::now();
        const double duration_since_start =
            seconds(chrono::now() - context.start_time).count();
        const size_t num_tasks_in_fly = context.num_tasks -
                                        context.completed_tasks.size() -
                                        context.remaining_tasks.size();
        // Assume tasks of other workers are halfly done in average.
        const double estimated_hours_left =
            duration_since_start / 3600 /
            (context.num_completed_tasks_since_start + num_tasks_in_fly * 0.5) *
            (context.remaining_tasks.size() + num_tasks_in_fly * 0.5);
        std::cout << (context.completed_tasks.size() + num_tasks_in_fly / 2)
                  << "/" << context.num_tasks << " (" << duration_since_start
                  << "s elapsed, ~" << estimated_hours_left << " hours left)"
                  << std::endl;
      }
    }
  }

  TaskInput current_task_input_;
  std::string metric_binary_folder_path_;
  EncodeMode encode_mode_ = EncodeMode::kEncode;
  StatusOr<TaskOutput> current_task_output_ = Status::kUnknownError;
  std::string serialized_current_task_output_;
  bool quiet_;
};

StatusOr<std::vector<TaskOutput>> LoadTasks(
    const ComparisonSettings& settings,
    const std::string& completed_tasks_file_path) {
  std::vector<TaskOutput> completed_tasks;
  if (std::filesystem::exists(completed_tasks_file_path)) {
    std::ifstream previous_completed_tasks_file(completed_tasks_file_path);
    if (!previous_completed_tasks_file.is_open()) {
      std::cerr << "Error: Could not open " << completed_tasks_file_path
                << " for reading" << std::endl;
      return Status::kUnknownError;
    }

    std::string line;
    while (std::getline(previous_completed_tasks_file, line)) {
      ASSIGN_OR_RETURN(
          const TaskOutput task_output,
          settings.discard_distortion_values
              ? TaskOutput::UnserializeNoDistortion(line, settings.quiet)
              : TaskOutput::Unserialize(line, settings.quiet));
      completed_tasks.push_back(task_output);
    }
    previous_completed_tasks_file.close();

    if (!settings.quiet) {
      std::cout << "Loaded " << completed_tasks.size() << " tasks from "
                << completed_tasks_file_path << std::endl;
    }
  }
  return completed_tasks;
}

Status ComputeDistortionInCompletedTasks(
    const ComparisonSettings& settings,
    std::vector<TaskOutput>& completed_tasks) {
  if (!settings.quiet) {
    std::cout << "Discarding read distortion values and recomputing them"
              << std::endl;
  }

  // Run everything but the encodings.
  WorkerContext context;
  context.load_encoded_from_disk = true;
  {
    // Only recompute distortion values once for each unique encoded path.
    std::unordered_set<std::string_view> encoded_paths;
    for (const TaskOutput& completed_task : completed_tasks) {
      CHECK_OR_RETURN(!completed_task.task_input.encoded_path.empty(),
                      settings.quiet);
      if (encoded_paths.insert(completed_task.task_input.encoded_path).second) {
        context.remaining_tasks.push_back(completed_task.task_input);
      }
    }
  }
  context.quiet = settings.quiet;
  context.num_tasks = context.remaining_tasks.size();
  context.metric_binary_folder_path = settings.metric_binary_folder_path;

  WorkerPool<WorkerContext, TaskWorker> pool(1 + settings.num_extra_threads);
  pool.Run(context);
  CHECK_OR_RETURN(context.completed_tasks.size() == context.num_tasks,
                  settings.quiet);

  {
    std::unordered_map<std::string_view, const TaskOutput*> results;
    // Reference the new distortions by encoded path.
    for (const TaskOutput& result : context.completed_tasks) {
      const bool is_unique =
          results.insert({result.task_input.encoded_path, &result}).second;
      CHECK_OR_RETURN(is_unique, settings.quiet);
    }
    // Copy the new distortions to the old completed_tasks. Keep the other old
    // metrics as is (encode timing etc.).
    for (TaskOutput& completed_task : completed_tasks) {
      const auto it = results.find(completed_task.task_input.encoded_path);
      CHECK_OR_RETURN(it != results.end(), settings.quiet);
      std::copy(it->second->distortions,
                it->second->distortions + kNumDistortionMetrics,
                completed_task.distortions);
    }
  }

  if (!settings.quiet) {
    std::cout << "Done recomputing distortion values" << std::endl;
  }
  return Status::kOk;
}

Status RemoveCompletedTasksFromRemainingTasks(
    const ComparisonSettings& settings,
    const std::string& completed_tasks_file_path,
    std::vector<TaskOutput>& completed_tasks,
    std::vector<TaskInput>& remaining_tasks) {
  CHECK_OR_RETURN(completed_tasks.size() <= remaining_tasks.size(),
                  settings.quiet)
      << "There are " << completed_tasks.size() << " tasks in "
      << completed_tasks_file_path << " but only " << remaining_tasks.size()
      << " were planned according to input flags";

  struct TaskInputComp {
    bool operator()(const TaskInput& a, const TaskInput& b) const {
      if (a.codec_settings.codec < b.codec_settings.codec) return true;
      if (a.codec_settings.codec > b.codec_settings.codec) return false;
      if (a.codec_settings.effort < b.codec_settings.effort) return true;
      if (a.codec_settings.effort > b.codec_settings.effort) return false;
      if (a.codec_settings.quality < b.codec_settings.quality) return true;
      if (a.codec_settings.quality > b.codec_settings.quality) return false;
      return a.image_path < b.image_path;
      // Ignore encoded_path which should depend on other fields.
    }
    bool operator()(const TaskOutput& a, const TaskOutput& b) const {
      return operator()(a.task_input, b.task_input);
    }
  } comp;

  // Using sorted tasks speeds lookups up.
  // An unordered map may be faster but hash function and map manipulation are
  // not as convenient and it is fast enough as is for now.
  std::sort(completed_tasks.begin(), completed_tasks.end(), comp);
  std::sort(remaining_tasks.begin(), remaining_tasks.end(), comp);
  std::vector<TaskInput> kept_remaining_tasks;
  kept_remaining_tasks.reserve(remaining_tasks.size() - completed_tasks.size());

  std::vector<TaskInput>::iterator it_remaining_task = remaining_tasks.begin();
  for (const TaskOutput& completed : completed_tasks) {
    while (it_remaining_task != remaining_tasks.end() &&
           !(*it_remaining_task == completed.task_input)) {
      kept_remaining_tasks.push_back(*it_remaining_task++);
    }
    CHECK_OR_RETURN(it_remaining_task != remaining_tasks.end(), settings.quiet)
        << "The following from " << completed_tasks_file_path
        << " does not match the input flags:" << completed.Serialize();
    ++it_remaining_task;
  }
  kept_remaining_tasks.insert(kept_remaining_tasks.end(), it_remaining_task,
                              remaining_tasks.end());

  assert(kept_remaining_tasks.size() ==
         remaining_tasks.size() - completed_tasks.size());
  remaining_tasks = std::move(kept_remaining_tasks);
  return Status::kOk;
}

Status ShuffleRemainingTasks(const ComparisonSettings& settings,
                             std::vector<TaskInput>& remaining_tasks) {
  if (settings.random_order) {
    // Uniform distribution of tasks to get as fair timings as possible.
    std::random_device rd;
    std::shuffle(remaining_tasks.begin(), remaining_tasks.end(),
                 std::mt19937(rd()));
  } else {
    // The tasks will be assigned starting at the back of the vector.
    // Reverse the order to execute in the same order as given in args.
    std::reverse(remaining_tasks.begin(), remaining_tasks.end());
  }
  return Status::kOk;
}

}  // namespace

Status Compare(const std::vector<std::string>& image_paths,
               const ComparisonSettings& settings,
               const std::string& completed_tasks_file_path,
               const std::string& results_folder_path) {
  CHECK_OR_RETURN(!completed_tasks_file_path.empty(), settings.quiet)
      << "No specified file path to store the progress";
  CHECK_OR_RETURN(!results_folder_path.empty(), settings.quiet)
      << "No specified folder path to store the results";

  WorkerContext context;
  ASSIGN_OR_RETURN(context.remaining_tasks, PlanTasks(image_paths, settings));
  ASSIGN_OR_RETURN(context.completed_tasks,
                   LoadTasks(settings, completed_tasks_file_path));
  if (settings.discard_distortion_values) {
    // Backup the old CSV file.
    std::filesystem::rename(completed_tasks_file_path,
                            completed_tasks_file_path + ".bck");
    OK_OR_RETURN(
        ComputeDistortionInCompletedTasks(settings, context.completed_tasks));
    // Dump the updated entries.
    std::ofstream completed_tasks_file(completed_tasks_file_path,
                                       std::ios::trunc);
    CHECK_OR_RETURN(completed_tasks_file.is_open(), settings.quiet)
        << "Could not open " << completed_tasks_file_path << " for writing";
    for (const TaskOutput& completed_task : context.completed_tasks) {
      completed_tasks_file << completed_task.Serialize() << std::endl;
    }
  }
  OK_OR_RETURN(RemoveCompletedTasksFromRemainingTasks(
      settings, completed_tasks_file_path, context.completed_tasks,
      context.remaining_tasks));
  OK_OR_RETURN(ShuffleRemainingTasks(settings, context.remaining_tasks));
  context.quiet = settings.quiet;
  context.num_tasks =
      context.completed_tasks.size() + context.remaining_tasks.size();

  context.completed_tasks_file_path = completed_tasks_file_path;
  context.completed_tasks_file.open(completed_tasks_file_path, std::ios::app);
  CHECK_OR_RETURN(context.completed_tasks_file.is_open(), settings.quiet)
      << "Could not open " << completed_tasks_file_path << " for writing";
  context.metric_binary_folder_path = settings.metric_binary_folder_path;

  if (!settings.quiet) {
    std::cout << "Starting " << context.remaining_tasks.size() << " tasks"
              << std::endl;
  }

  const chrono::time_point start_time = chrono::now();

  WorkerPool<WorkerContext, TaskWorker> pool(1 + settings.num_extra_threads);
  pool.Run(context);
  context.completed_tasks_file.close();
  if (context.num_failures > kMaxNumFailures ||
      context.num_completed_tasks_since_start == 0) {
    OK_OR_RETURN(context.status);
  }

  std::vector<std::vector<TaskOutput>> results;
  ASSIGN_OR_RETURN(results, SplitByCodecSettingsAndAggregateByImageAndQuality(
                                context.completed_tasks, settings.quiet));
  for (const std::vector<TaskOutput>& tasks : results) {
    const CodecSettings& codec_settings =
        tasks.front().task_input.codec_settings;
    const std::string batch_name = CodecName(codec_settings.codec) + "_" +
                                   std::to_string(codec_settings.effort);
    OK_OR_RETURN(TasksToJson(
        batch_name, codec_settings, tasks, settings.quiet,
        std::filesystem::path(results_folder_path) / (batch_name + ".json")));
  }

  // TODO(yguyon): std::cout a quick summary of the comparison results
  //               (overall arith and geo means).

  if (!settings.quiet) {
    const double duration = seconds(chrono::now() - start_time).count();
    double total_duration = 0;
    uint64_t total_encoded_size = 0;
    for (const TaskOutput& task_output : context.completed_tasks) {
      total_duration +=
          task_output.encoding_duration + task_output.decoding_duration;
      total_encoded_size += task_output.encoded_size;
    }
    std::cout << "Took " << duration
              << " seconds (estimated overall duration: " << total_duration
              << " seconds / " << (settings.num_extra_threads + 1)
              << " threads = "
              << total_duration / (settings.num_extra_threads + 1)
              << " seconds of encoding/decoding)" << std::endl;
    std::cout << "Represents " << total_encoded_size
              << " encoded bytes (average: "
              << static_cast<float>(total_encoded_size) /
                     context.completed_tasks.size()
              << "B per encoding)" << std::endl;
    if (context.num_failures > 0) {
      std::cout << " /!\\ Warning: " << context.num_failures << " failures"
                << std::endl;
    }
  }
  return Status::kOk;
}

}  // namespace codec_compare_gen
