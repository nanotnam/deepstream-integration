#include "deepstream_runtime/job_queue.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace deepstream_runtime {

PlateJobQueue::PlateJobQueue(size_t capacity) : capacity_(capacity) {
  if (capacity == 0U) throw std::invalid_argument("queue capacity must be positive");
}

bool PlateJobQueue::push(PlateWorkItem item) {
  std::unique_lock<std::mutex> lock(mutex_);
  const bool blocked = jobs_.size() == capacity_ && !closed_;
  const auto wait_started = std::chrono::steady_clock::now();
  if (blocked) ++blocked_pushes_;
  not_full_.wait(lock, [&] { return jobs_.size() < capacity_ || closed_; });
  if (blocked) {
    blocked_nanoseconds_ += static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - wait_started).count());
  }
  if (closed_) return false;
  jobs_.push_back({std::move(item), std::chrono::steady_clock::now()});
  ++jobs_pushed_;
  maximum_depth_ = std::max(maximum_depth_, jobs_.size());
  lock.unlock();
  not_empty_.notify_one();
  return true;
}

std::vector<PlateWorkItem> PlateJobQueue::pop_batch(size_t maximum_batch_size) {
  if (maximum_batch_size == 0U) {
    throw std::invalid_argument("maximum batch size must be positive");
  }
  std::unique_lock<std::mutex> lock(mutex_);
  not_empty_.wait(lock, [&] { return !jobs_.empty() || closed_; });
  const size_t count = std::min(maximum_batch_size, jobs_.size());
  std::vector<PlateWorkItem> batch;
  batch.reserve(count);
  for (size_t index = 0U; index < count; ++index) {
    batch.push_back(std::move(jobs_.front().item));
    jobs_.pop_front();
  }
  jobs_popped_ += count;
  lock.unlock();
  if (count > 0U) not_full_.notify_all();
  return batch;
}

void PlateJobQueue::close() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
  }
  not_empty_.notify_all();
  not_full_.notify_all();
}

JobQueueSnapshot PlateJobQueue::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  const uint64_t oldest_age = jobs_.empty()
      ? 0U
      : static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - jobs_.front().enqueued_at).count());
  return {capacity_, jobs_.size(), maximum_depth_, jobs_pushed_, jobs_popped_,
          blocked_pushes_, blocked_nanoseconds_, oldest_age, closed_};
}

}  // namespace deepstream_runtime
