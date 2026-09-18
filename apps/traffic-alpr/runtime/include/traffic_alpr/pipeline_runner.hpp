#pragma once

#include "traffic_alpr/config.hpp"

#include <atomic>
#include <memory>
#include <string>

namespace traffic_alpr {

class PipelineRunner {
 public:
  explicit PipelineRunner(PipelineConfig config);
  ~PipelineRunner();
  PipelineRunner(const PipelineRunner&) = delete;
  PipelineRunner& operator=(const PipelineRunner&) = delete;

  int run(std::string* error);
  void request_stop();

 private:
  class Implementation;
  std::unique_ptr<Implementation> implementation_;
};

}  // namespace traffic_alpr
