#pragma once

#include "alpr/types.hpp"
#include "deepstream_platform/frame_resource.hpp"

#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace traffic_alpr {

using FrameResource = deepstream_platform::FrameResource;
using FrameLease = deepstream_platform::FrameLease;

struct PlateWorkItem {
  alpr::PlateJob job;
  FrameLease frame;
};

struct JobQueueSnapshot {
  size_t capacity{0};
  size_t depth{0};
  size_t maximum_depth{0};
  uint64_t jobs_pushed{0};
  uint64_t jobs_popped{0};
  uint64_t blocked_pushes{0};
  uint64_t blocked_nanoseconds{0};
  uint64_t oldest_job_age_nanoseconds{0};
  bool closed{false};
};

class PlateJobQueue {
 public:
  explicit PlateJobQueue(size_t capacity);
  PlateJobQueue(const PlateJobQueue&) = delete;
  PlateJobQueue& operator=(const PlateJobQueue&) = delete;

  bool push(PlateWorkItem item);
  std::vector<PlateWorkItem> pop_batch(size_t maximum_batch_size);
  void close();
  JobQueueSnapshot snapshot() const;

 private:
  const size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  struct QueuedJob {
    PlateWorkItem item;
    std::chrono::steady_clock::time_point enqueued_at;
  };
  std::deque<QueuedJob> jobs_;
  size_t maximum_depth_{0};
  uint64_t jobs_pushed_{0};
  uint64_t jobs_popped_{0};
  uint64_t blocked_pushes_{0};
  uint64_t blocked_nanoseconds_{0};
  bool closed_{false};
};

}  // namespace traffic_alpr
