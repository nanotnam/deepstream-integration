#include "deepstream_runtime/plate_worker.hpp"

#include <stdexcept>
#include <utility>

namespace deepstream_runtime {

PlateWorker::PlateWorker(ProcessingCoordinator* coordinator,
                         PlateBatchHandler handler)
    : coordinator_(coordinator), handler_(std::move(handler)) {
  if (coordinator_ == nullptr || !handler_) {
    throw std::invalid_argument("plate worker requires a coordinator and handler");
  }
}

PlateWorker::~PlateWorker() { stop(); }

void PlateWorker::start() {
  if (thread_.joinable()) return;
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) return;
  thread_ = std::thread(&PlateWorker::run, this);
}

void PlateWorker::stop() {
  if (!running_.exchange(false) && !thread_.joinable()) return;
  coordinator_->close();
  if (thread_.joinable()) thread_.join();
}

bool PlateWorker::running() const { return running_.load(); }

uint64_t PlateWorker::failed_batches() const { return failed_batches_.load(); }

void PlateWorker::run() {
  while (true) {
    std::vector<PlateWorkItem> batch = coordinator_->take_plate_batch();
    if (batch.empty()) break;
    size_t failures = 0U;
    try {
      failures = handler_(batch);
      if (failures > batch.size()) failures = batch.size();
    } catch (...) {
      failures = batch.size();
      ++failed_batches_;
    }
    coordinator_->complete_plate_batch(batch, failures);
  }
  running_.store(false);
}

}  // namespace deepstream_runtime
