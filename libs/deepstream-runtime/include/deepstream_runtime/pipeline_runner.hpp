#pragma once

#include "deepstream_runtime/config.hpp"

#include <atomic>
#include <memory>
#include <string>

namespace deepstream_runtime {

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

}  // namespace deepstream_runtime
