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

#include "src/distortion.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(HAVE_UNISTD_H) && !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <crt_externs.h>
#endif
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#if defined(HAVE_UNISTD_H) && !defined(__EMSCRIPTEN__)
#include <cerrno>
#include <mutex>
#include <system_error>
#endif

#include "imageio/image_enc.h"
#include "src/base.h"
#include "src/codec.h"
#include "src/frame.h"
#include "src/framework.h"
#include "src/serialization.h"
#include "src/task.h"
#include "src/wp2/base.h"

#if defined(HAVE_UNISTD_H) && !defined(__EMSCRIPTEN__) && !defined(__APPLE__)
extern "C" {
extern char** environ;
}
#endif

namespace codec_compare_gen {

namespace {

class FileDeleter {
 public:
  explicit FileDeleter(const std::string& file_path) : file_path_(file_path) {}
  ~FileDeleter() {
    if (!file_path_.empty()) std::filesystem::remove(file_path_);
  }
  const std::string file_path_;
};

std::string AsPathArg(std::string_view path) {
  // Prefix relative paths starting with '-' with "./" so external binaries do
  // not parse them as command-line flags.
  if (!path.empty() && path.front() == '-') {
    return "./" + std::string(path);
  }
  return std::string(path);
}

#if defined(HAVE_UNISTD_H) && !defined(__EMSCRIPTEN__)

std::string ErrnoMessage(int err) {
  return std::error_code(err, std::generic_category()).message();
}

class ScopedFd {
 public:
  explicit ScopedFd(int fd = -1) : fd_(fd) {}
  ~ScopedFd() { Close(); }
  ScopedFd(const ScopedFd&) = delete;
  ScopedFd& operator=(const ScopedFd&) = delete;

  int get() const { return fd_; }
  void Reset(int fd = -1) {
    if (fd_ == fd) return;
    if (fd_ >= 0) close(fd_);
    fd_ = fd;
  }
  void Close() { Reset(-1); }

 private:
  int fd_;
};

class ScopedSpawnFileActions {
 public:
  ScopedSpawnFileActions() = default;
  ~ScopedSpawnFileActions() {
    if (initialized_) posix_spawn_file_actions_destroy(&actions_);
  }
  ScopedSpawnFileActions(const ScopedSpawnFileActions&) = delete;
  ScopedSpawnFileActions& operator=(const ScopedSpawnFileActions&) = delete;

  int Init() {
    const int err = posix_spawn_file_actions_init(&actions_);
    if (err == 0) initialized_ = true;
    return err;
  }
  posix_spawn_file_actions_t* get() { return &actions_; }

 private:
  posix_spawn_file_actions_t actions_;
  bool initialized_ = false;
};

// Runs the binary directly in a sub-process without invoking a shell and
// returns its standard output.
StatusOr<std::string> RunProcess(const std::string& binary_path,
                                 const std::vector<std::string>& args,
                                 bool quiet) {
  CHECK_OR_RETURN(binary_path.find('\0') == std::string::npos, quiet)
      << "Null byte in binary path";
  for (const std::string& arg : args) {
    CHECK_OR_RETURN(arg.find('\0') == std::string::npos, quiet)
        << "Null byte in process argument";
  }

  pid_t pid = -1;
  ScopedFd read_fd;
  {
    // Serialize pipe creation, FD_CLOEXEC setup, and posix_spawn() so that
    // another thread's child process cannot inherit this thread's pipe write
    // end (which would keep the pipe open after this child exits).
    static auto* const spawn_mutex = new std::mutex();
    std::lock_guard<std::mutex> lock(*spawn_mutex);

    int pipe_fds[2];
    CHECK_OR_RETURN(pipe(pipe_fds) == 0, quiet)
        << "pipe() failed: " << ErrnoMessage(errno);
    read_fd.Reset(pipe_fds[0]);
    ScopedFd write_fd(pipe_fds[1]);

    CHECK_OR_RETURN(fcntl(read_fd.get(), F_SETFD, FD_CLOEXEC) == 0, quiet)
        << "fcntl(FD_CLOEXEC) failed: " << ErrnoMessage(errno);
    if (write_fd.get() != STDOUT_FILENO) {
      CHECK_OR_RETURN(fcntl(write_fd.get(), F_SETFD, FD_CLOEXEC) == 0, quiet)
          << "fcntl(FD_CLOEXEC) failed: " << ErrnoMessage(errno);
    }

    ScopedSpawnFileActions file_actions;
    int err = file_actions.Init();
    CHECK_OR_RETURN(err == 0, quiet)
        << "posix_spawn_file_actions_init() failed: " << ErrnoMessage(err);

    err = posix_spawn_file_actions_addclose(file_actions.get(), read_fd.get());
    CHECK_OR_RETURN(err == 0, quiet)
        << "posix_spawn_file_actions_addclose() failed: " << ErrnoMessage(err);

    if (write_fd.get() != STDOUT_FILENO) {
      err = posix_spawn_file_actions_adddup2(file_actions.get(), write_fd.get(),
                                             STDOUT_FILENO);
      CHECK_OR_RETURN(err == 0, quiet)
          << "posix_spawn_file_actions_adddup2() failed: " << ErrnoMessage(err);
      err =
          posix_spawn_file_actions_addclose(file_actions.get(), write_fd.get());
      CHECK_OR_RETURN(err == 0, quiet)
          << "posix_spawn_file_actions_addclose() failed: "
          << ErrnoMessage(err);
    }

    std::string binary = binary_path;
    std::vector<std::string> argv_storage = args;
    std::vector<char*> argv;
    argv.reserve(argv_storage.size() + 2);
    argv.push_back(binary.data());
    for (std::string& arg : argv_storage) {
      argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

#ifdef __APPLE__
    char** const envp = *_NSGetEnviron();
#else
    char** const envp = environ;
#endif

    err = posix_spawn(&pid, binary.c_str(), file_actions.get(),
                      /*attrp=*/nullptr, argv.data(), envp);
    CHECK_OR_RETURN(err == 0, quiet)
        << "posix_spawn(" << binary_path << ") failed: " << ErrnoMessage(err);
  }

  std::string standard_output;
  char buffer[4096];
  bool read_failed = false;
  int read_errno = 0;
  for (;;) {
    const ssize_t bytes_read = read(read_fd.get(), buffer, sizeof(buffer));
    if (bytes_read > 0) {
      standard_output.append(buffer, static_cast<size_t>(bytes_read));
    } else if (bytes_read == 0) {
      break;
    } else if (errno != EINTR) {
      read_failed = true;
      read_errno = errno;
      break;
    }
  }
  read_fd.Close();

  int wait_status = 0;
  pid_t waited_pid;
  do {
    waited_pid = waitpid(pid, &wait_status, 0);
  } while (waited_pid < 0 && errno == EINTR);

  CHECK_OR_RETURN(waited_pid == pid, quiet)
      << "waitpid(" << binary_path << ") failed: " << ErrnoMessage(errno);
  CHECK_OR_RETURN(!read_failed, quiet)
      << "read(" << binary_path << ") failed: " << ErrnoMessage(read_errno);
  if (WIFEXITED(wait_status)) {
    CHECK_OR_RETURN(WEXITSTATUS(wait_status) == 0, quiet)
        << binary_path << " exited with status " << WEXITSTATUS(wait_status);
  } else if (WIFSIGNALED(wait_status)) {
    CHECK_OR_RETURN(false, quiet)
        << binary_path << " terminated by signal " << WTERMSIG(wait_status);
  } else {
    CHECK_OR_RETURN(false, quiet)
        << binary_path << " did not exit normally (status " << wait_status
        << ")";
  }
  return standard_output;
}

#else

StatusOr<std::string> RunProcess(const std::string& binary_path,
                                 const std::vector<std::string>& args,
                                 bool quiet) {
  (void)binary_path;
  (void)args;
  CHECK_OR_RETURN(false, quiet)
      << "Running external binaries is not supported on this platform";
  return Status::kUnknownError;
}

#endif

//------------------------------------------------------------------------------

StatusOr<float> GetLibwebp2Distortion(const WP2::ArgbBuffer& reference,
                                      const WP2::ArgbBuffer& image,
                                      const TaskInput& task,
                                      WP2::MetricType metric, bool quiet) {
  float distortion[5];
  const WP2Status status =
      (task.codec_settings.quality == kQualityLossless ||
       !reference.HasTransparency())
          ? image.GetDistortion(reference, metric, distortion)
          : image.GetDistortionBlackOrWhiteBackground(reference, metric,
                                                      distortion);

  if (status == WP2_STATUS_UNSUPPORTED_FEATURE &&
      WP2FormatBpp(reference.format()) != 4) {
    // Some metrics need four channels.
    WP2::ArgbBuffer reference4(
        WP2IsPremultiplied(reference.format()) ? WP2_Argb_32 : WP2_ARGB_32);
    CHECK_OR_RETURN(reference4.ConvertFrom(reference) == WP2_STATUS_OK, quiet);
    WP2::ArgbBuffer image4(WP2IsPremultiplied(image.format()) ? WP2_Argb_32
                                                              : WP2_ARGB_32);
    CHECK_OR_RETURN(image4.ConvertFrom(image) == WP2_STATUS_OK, quiet);
    return GetLibwebp2Distortion(reference4, image4, task, metric, quiet);
  }

  CHECK_OR_RETURN(status == WP2_STATUS_OK, quiet)
      << "GetDistortion(" << metric << ") failed on " << task.image_path
      << " and " << GetCodecMetadata(task.codec_settings.codec).name
      << " at effort " << task.codec_settings.effort << ", chroma subsampling "
      << SubsamplingToString(task.codec_settings.chroma_subsampling)
      << " and quality " << task.codec_settings.quality << ": "
      << WP2GetStatusMessage(status);
  float overall_distortion = distortion[4];

  if (metric == WP2::PSNR &&
      (task.codec_settings.quality == kQualityLossless
           ? overall_distortion != kNoDistortion
           : overall_distortion <
                 (task.codec_settings.quality > 90 ? 10 : 2))) {
    if (!quiet) {
      std::cerr << "Error: " << task.image_path
                << " was encoded or decoded with loss in "
                << GetCodecMetadata(task.codec_settings.codec).name
                << " format at effort " << task.codec_settings.effort
                << ", chroma subsampling "
                << SubsamplingToString(task.codec_settings.chroma_subsampling)
                << " and quality " << task.codec_settings.quality << " (alpha "
                << distortion[0] << "dB, R " << distortion[1] << "dB, G "
                << distortion[2] << "dB, B " << distortion[3] << "dB, overall "
                << overall_distortion << "dB)" << std::endl;
    }
    // Uncomment to dump the problematic image.
#if 0
    (void)WP2::SaveImage(reference, "/tmp/ccgen_original.png");
    (void)WP2::SaveImage(image, "/tmp/ccgen_decoded.png");
#endif
    return Status::kUnknownError;
  }
  return overall_distortion;
}

Status SaveImage(const WP2::ArgbBuffer& image, const std::string& file_path,
                 bool quiet) {
  const WP2Status status =
      WP2::SaveImage(image, file_path.c_str(), /*overwrite=*/true);
  if (status == WP2_STATUS_UNSUPPORTED_FEATURE &&
      image.format() != WP2_Argb_32 && image.format() != WP2_ARGB_32) {
    WP2::ArgbBuffer image4(WP2IsPremultiplied(image.format()) ? WP2_Argb_32
                                                              : WP2_ARGB_32);
    OK_WP2_OR_RETURN(image4.ConvertFrom(image), quiet);
    return ::codec_compare_gen::SaveImage(image4, file_path, quiet);
  }
  OK_WP2_OR_RETURN(status, quiet) << "SaveImage(" << file_path << ") failed";
  return Status::kOk;
}

Status ScaleTill8x8(const WP2::ArgbBuffer& from, WP2::ArgbBuffer& to,
                    bool quiet) {
  OK_WP2_OR_RETURN(to.SetFormat(from.format()), quiet);
  OK_WP2_OR_RETURN(
      to.Resize(std::max(from.width(), 8u), std::max(from.height(), 8u)),
      quiet);
  for (uint32_t to_y = 0; to_y < to.height(); ++to_y) {
    const uint32_t from_y = to_y * (from.height() - 1) / (to.height() - 1);
    for (uint32_t to_x = 0; to_x < to.width(); ++to_x) {
      const uint32_t from_x = to_x * (from.width() - 1) / (to.width() - 1);
      if (WP2Formatbpc(to.format()) == 8) {
        to.GetRow8(to_y)[to_x] = from.GetRow8(from_y)[from_x];
      } else {
        to.GetRow16(to_y)[to_x] = from.GetRow16(from_y)[from_x];
      }
    }
  }
  return Status::kOk;
}

StatusOr<std::string> GetBinaryDistortion(
    const std::string& reference_path, const WP2::ArgbBuffer& reference,
    const std::string& image_path, const WP2::ArgbBuffer& image,
    const TaskInput& task, const std::string& metric_binary_path,
    bool metric_supports_tiny_dimensions, size_t thread_id, bool quiet) {
  CHECK_OR_RETURN(!reference_path.empty(), quiet);
  CHECK_OR_RETURN(!metric_binary_path.empty(), quiet);
  const bool maybeAnimated = !EndsWith(reference_path, ".png");

  std::string path_prefix;
  if (image_path.empty() || maybeAnimated) {
    path_prefix = std::filesystem::temp_directory_path() / "codec_compare_gen";
#ifdef HAVE_UNISTD_H
    // Process-safe file name.
    path_prefix += "_process";
    path_prefix += std::to_string(getpid());
#endif
    // Thread-safe file name.
    path_prefix += "_thread";
    path_prefix += std::to_string(thread_id);
  }

  WP2::ArgbBuffer final_reference(reference.format());
  WP2::ArgbBuffer final_image(image.format());
  if (metric_supports_tiny_dimensions ||
      (reference.width() >= 8 && reference.height() >= 8)) {
    OK_WP2_OR_RETURN(final_reference.SetView(reference), quiet);
    OK_WP2_OR_RETURN(final_image.SetView(image), quiet);
  } else {
    OK_OR_RETURN(ScaleTill8x8(reference, final_reference, quiet));
    OK_OR_RETURN(ScaleTill8x8(image, final_image, quiet));
  }

  // Create a PNG file containing the original pixels of the current frame if
  // not PNG (could be a GIF with multiple frames for example).
  std::string temp_reference_path;
  std::string_view final_reference_path = reference_path;
  if (!metric_supports_tiny_dimensions || maybeAnimated) {
    temp_reference_path = path_prefix + "_reference.png";
    OK_OR_RETURN(SaveImage(final_reference, temp_reference_path, quiet));
    final_reference_path = temp_reference_path;
  }
  const FileDeleter temp_reference_path_deleter(temp_reference_path);

  // Create a PNG file containing the decoded pixels if not done or if animated.
  std::string temp_image_path;
  std::string_view final_image_path = image_path;
  if (!metric_supports_tiny_dimensions || image_path.empty() || maybeAnimated) {
    temp_image_path = path_prefix + "_image.png";
    OK_OR_RETURN(SaveImage(final_image, temp_image_path, quiet));
    final_image_path = temp_image_path;
  }
  const FileDeleter temp_image_path_deleter(temp_image_path);

  return RunProcess(
      metric_binary_path,
      {AsPathArg(final_reference_path), AsPathArg(final_image_path)}, quiet);
}

StatusOr<float> GetLibjxlDistortion(
    const std::string& reference_path, const WP2::ArgbBuffer& reference,
    const std::string& image_path, const WP2::ArgbBuffer& image,
    const TaskInput& task, const std::string& metric_binary_folder_path,
    DistortionMetric metric, size_t thread_id, bool quiet) {
  // Metric binaries are not available: just return -1 for simplicity.
  if (metric_binary_folder_path.empty()) return -1;

  const char* metric_binary_name;
  bool metric_supports_tiny_dimensions;
  if (metric == DistortionMetric::kLibjxlButteraugli ||
      metric == DistortionMetric::kLibjxlP3norm) {
    metric_binary_name = "butteraugli_main";
    metric_supports_tiny_dimensions = true;
  } else if (metric == DistortionMetric::kLibjxlSsimulacra) {
    metric_binary_name = "ssimulacra_main";
    metric_supports_tiny_dimensions = false;
  } else {
    CHECK_OR_RETURN(metric == DistortionMetric::kLibjxlSsimulacra2, quiet);
    metric_binary_name = "ssimulacra2";
    metric_supports_tiny_dimensions = false;
  }
  const std::string metric_binary_path =
      std::filesystem::path(metric_binary_folder_path) / "_deps" /
      "libjxl-build" / "tools" / metric_binary_name;
  ASSIGN_OR_RETURN(
      std::string standard_output,
      GetBinaryDistortion(reference_path, reference, image_path, image, task,
                          metric_binary_path, metric_supports_tiny_dimensions,
                          thread_id, quiet));

  if (metric == DistortionMetric::kLibjxlButteraugli ||
      metric == DistortionMetric::kLibjxlP3norm) {
    static constexpr const char* kP3normToken = "3-norm:";
    const auto p3norm_token_position = standard_output.find(kP3normToken);
    CHECK_OR_RETURN(p3norm_token_position != std::string::npos, quiet)
        << "\"3-norm:\" token not found in \"" << standard_output << "\"";
    standard_output = metric == DistortionMetric::kLibjxlButteraugli
                          ? standard_output.substr(0, p3norm_token_position)
                          : standard_output.substr(p3norm_token_position +
                                                   std::strlen(kP3normToken));
  }

  return std::stof(Trim(standard_output));
}

StatusOr<float> GetDssimDistortion(const std::string& reference_path,
                                   const WP2::ArgbBuffer& reference,
                                   const std::string& image_path,
                                   const WP2::ArgbBuffer& image,
                                   const TaskInput& task,
                                   const std::string& metric_binary_folder_path,
                                   size_t thread_id, bool quiet) {
  // Metric binaries are not available: just return -1 for simplicity.
  if (metric_binary_folder_path.empty()) return -1;

  const std::string metric_binary_path =
      std::filesystem::path(metric_binary_folder_path) / "_deps" / "dssim-src" /
      "target" / "release" / "dssim";
  const bool metric_supports_tiny_dimensions = true;
  ASSIGN_OR_RETURN(
      const std::string standard_output,
      GetBinaryDistortion(reference_path, reference, image_path, image, task,
                          metric_binary_path, metric_supports_tiny_dimensions,
                          thread_id, quiet));
  return std::stof(Trim(Split(standard_output, '\t').front()));
}

StatusOr<float> GetDistortion(
    const std::string& reference_path, const WP2::ArgbBuffer& reference,
    const std::string& image_path, const WP2::ArgbBuffer& image,
    const TaskInput& task, const std::string& metric_binary_folder_path,
    DistortionMetric metric, size_t thread_id, bool quiet) {
  if (metric_binary_folder_path == "no_metric_binary_for_testing" &&
      metric != DistortionMetric::kLibwebp2Psnr) {
    // Return a placeholder value which does not need actual distortion binaries
    // for testing.
    return GetLibwebp2Distortion(reference, image, task, WP2::PSNR, quiet);
  }

  switch (metric) {
    case DistortionMetric::kLibwebp2Psnr:
      return GetLibwebp2Distortion(reference, image, task, WP2::PSNR, quiet);
    case DistortionMetric::kLibwebp2Ssim:
      return GetLibwebp2Distortion(reference, image, task, WP2::SSIM, quiet);
    case DistortionMetric::kLibjxlButteraugli:
    case DistortionMetric::kLibjxlSsimulacra:
    case DistortionMetric::kLibjxlSsimulacra2:
    case DistortionMetric::kLibjxlP3norm:
      return GetLibjxlDistortion(reference_path, reference, image_path, image,
                                 task, metric_binary_folder_path, metric,
                                 thread_id, quiet);
    case DistortionMetric::kDssim:
      return GetDssimDistortion(reference_path, reference, image_path, image,
                                task, metric_binary_folder_path, thread_id,
                                quiet);
  }
  return Status::kUnknownError;
}

}  // namespace

StatusOr<float> GetAverageDistortion(
    const std::string& a_path, const Image& a, const std::string& b_path,
    const Image& b, const TaskInput& task,
    const std::string& metric_binary_folder_path, DistortionMetric metric,
    size_t thread_id, bool quiet) {
  CHECK_OR_RETURN(!a.empty() && !b.empty(), quiet);
  if (a.size() == 1 && b.size() == 1) {
    return GetDistortion(a_path, a.front().pixels, b_path, b.front().pixels,
                         task, metric_binary_folder_path, metric, thread_id,
                         quiet);
  }

  const uint32_t a_duration_ms = GetDurationMs(a);
  CHECK_OR_RETURN(a_duration_ms > 0, quiet);
  CHECK_OR_RETURN(a_duration_ms == GetDurationMs(b), quiet);

  float distortion_sum = 0;
  size_t a_index = 0, b_index = 0;
  uint32_t previous_time = 0, a_time = 0, b_time = 0;  // milliseconds
  do {
    ASSIGN_OR_RETURN(
        const float distortion,
        GetDistortion(a_path, a[a_index].pixels, b_path, b[b_index].pixels,
                      task, metric_binary_folder_path, metric, thread_id,
                      quiet));

    const uint32_t next_a_time = a_time + a[a_index].duration_ms;
    const uint32_t next_b_time = b_time + b[b_index].duration_ms;
    const uint32_t current_time = std::min(next_a_time, next_b_time);
    // Weigh the distortion by frame duration.
    distortion_sum += distortion * (current_time - previous_time);

    if (current_time >= next_a_time) {
      ++a_index;
      a_time = next_a_time;
    }
    if (current_time >= next_b_time) {
      ++b_index;
      b_time = next_b_time;
    }
    previous_time = current_time;
  } while (a_index < a.size() && b_index < b.size());
  CHECK_OR_RETURN(a_index == a.size() && b_index == b.size(), quiet);
  CHECK_OR_RETURN(a_time == b_time && a_time == a_duration_ms, quiet);
  return distortion_sum / a_duration_ms;
}

StatusOr<bool> PixelEquality(const WP2::ArgbBuffer& a, const WP2::ArgbBuffer& b,
                             bool quiet) {
  CHECK_OR_RETURN(a.format() == b.format(), quiet);
  CHECK_OR_RETURN(a.width() == b.width() && a.height() == b.height(), quiet);
  for (uint32_t y = 0; y < a.height(); ++y) {
    if (std::memcmp(a.GetRow(y), b.GetRow(y),
                    a.width() * WP2FormatBpp(a.format())) != 0) {
      return false;
    }
  }
  return true;
}

StatusOr<bool> PixelEquality(const Image& a, const Image& b, bool quiet) {
  CHECK_OR_RETURN(!a.empty() && !b.empty(), quiet);
  if (a.size() == 1 && b.size() == 1) {
    return PixelEquality(a.front().pixels, b.front().pixels, quiet);
  }

  const uint32_t a_duration_ms = GetDurationMs(a);
  CHECK_OR_RETURN(a_duration_ms > 0, quiet);
  CHECK_OR_RETURN(a_duration_ms == GetDurationMs(b), quiet);

  size_t a_index = 0, b_index = 0;
  uint32_t a_time = 0, b_time = 0;  // milliseconds
  do {
    ASSIGN_OR_RETURN(
        const bool pixel_equality,
        PixelEquality(a[a_index].pixels, b[b_index].pixels, quiet));
    if (!pixel_equality) return false;

    const uint32_t next_a_time = a_time + a[a_index].duration_ms;
    const uint32_t next_b_time = b_time + b[b_index].duration_ms;
    const uint32_t current_time = std::min(next_a_time, next_b_time);
    if (current_time >= next_a_time) {
      ++a_index;
      a_time = next_a_time;
    }
    if (current_time >= next_b_time) {
      ++b_index;
      b_time = next_b_time;
    }
  } while (a_index < a.size() && b_index < b.size());
  CHECK_OR_RETURN(a_index == a.size() && b_index == b.size(), quiet);
  CHECK_OR_RETURN(a_time == b_time && a_time == a_duration_ms, quiet);
  return true;
}

void GeometricMean::Add(double value) {
  product_ *= value;
  ++num_values_;

  // A large number of small or big values may lead to floating point
  // underflow or overflow. Avoid that by regularly dumping part of the
  // computation to a side accumulator.
  if (product_ < 1e-16 || product_ > 1e16) {
    product_log_sum_ += std::log(product_);
    product_ = 1;
  }
}

double GeometricMean::Get() const {
  if (num_values_ == 0) return 1;
  return std::exp((product_log_sum_ + std::log(product_)) /
                  static_cast<double>(num_values_));
}

}  // namespace codec_compare_gen
