#pragma once

#include "alpr/types.hpp"
#include "deepstream_runtime/processing.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

namespace deepstream_runtime {

using PlateBatchHandler =
    std::function<size_t(const std::vector<PlateWorkItem>& jobs)>;

class PlateWorker {
 public:
  PlateWorker(ProcessingCoordinator* coordinator, PlateBatchHandler handler);
  ~PlateWorker();
  PlateWorker(const PlateWorker&) = delete;
  PlateWorker& operator=(const PlateWorker&) = delete;

  void start();
  void stop();
  bool running() const;
  uint64_t failed_batches() const;

 private:
  void run();

  ProcessingCoordinator* coordinator_;
  PlateBatchHandler handler_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<uint64_t> failed_batches_{0};
};

}  // namespace deepstream_runtime
