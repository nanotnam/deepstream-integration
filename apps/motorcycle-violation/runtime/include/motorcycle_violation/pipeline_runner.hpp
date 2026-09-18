#pragma once

#include "motorcycle_violation/config.hpp"

#include <atomic>
#include <memory>
#include <string>

namespace motorcycle_violation {

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

}  // namespace motorcycle_violation
