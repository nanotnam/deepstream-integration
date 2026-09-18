#include "traffic_alpr/pipeline_runner.hpp"

#include <utility>

namespace traffic_alpr {

class PipelineRunner::Implementation {
 public:
  explicit Implementation(PipelineConfig config) : config_(std::move(config)) {}
  PipelineConfig config_;
};

PipelineRunner::PipelineRunner(PipelineConfig config)
    : implementation_(std::make_unique<Implementation>(std::move(config))) {}

PipelineRunner::~PipelineRunner() = default;

int PipelineRunner::run(std::string* error) {
  static_cast<void>(implementation_->config_);
  *error = "runtime unavailable: build inside the pinned DeepStream container for GPU execution";
  return 69;
}

void PipelineRunner::request_stop() {}

}  // namespace traffic_alpr
