#pragma once

#include "alpr/event.hpp"
#include "motorcycle_violation/config.hpp"
#include "motorcycle_violation/engine_set.hpp"
#include "motorcycle_violation/job_queue.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace motorcycle_violation {

class GpuInferenceExecutor {
 public:
  using EventHandler = std::function<void(const alpr::AlprEvent&)>;

  GpuInferenceExecutor(const PipelineConfig& config, const EngineSet& engines,
                       EventHandler event_handler);
  ~GpuInferenceExecutor();
  GpuInferenceExecutor(const GpuInferenceExecutor&) = delete;
  GpuInferenceExecutor& operator=(const GpuInferenceExecutor&) = delete;

  bool initialize(std::string* error);
  size_t process(const std::vector<PlateWorkItem>& jobs);
  void mark_seen(uint64_t track_id, uint64_t frame_number);
  std::vector<uint64_t> expire(uint64_t frame_number);
  std::vector<uint64_t> clear();
  std::string last_error() const;

 private:
  class Implementation;
  std::unique_ptr<Implementation> implementation_;
};

}  // namespace motorcycle_violation
