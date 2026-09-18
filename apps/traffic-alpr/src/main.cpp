#include "deepstream_runtime/config.hpp"
#include "deepstream_runtime/pipeline_description.hpp"
#include "deepstream_runtime/pipeline_runner.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void usage() {
  std::cerr << "usage: traffic-alpr --config <pipeline.yaml> "
               "[--validate-only|--print-gst-graph]\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string config_path;
  bool validate_only = false;
  bool print_graph = false;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--config" && index + 1 < argc) {
      config_path = argv[++index];
    } else if (argument == "--validate-only") {
      validate_only = true;
    } else if (argument == "--print-gst-graph") {
      print_graph = true;
    } else {
      usage();
      return 64;
    }
  }
  if (config_path.empty() || (validate_only && print_graph)) {
    usage();
    return 64;
  }

  deepstream_runtime::PipelineConfig config;
  std::string error;
  if (!deepstream_runtime::load_pipeline_config(config_path, &config, &error)) {
    std::cerr << "configuration error: " << error << '\n';
    return 65;
  }
  if (validate_only) {
    std::cout << "{\"schema\":\"mbfs.alpr.cli-result.v1\","
                 "\"status\":\"valid\",\"source_id\":\""
              << config.source.id << "\",\"processing\":{\"max_fps\":"
              << config.processing.max_fps << ",\"plate_batch_size\":"
              << config.processing.plate_batch_size << ",\"lpr_batch_size\":"
              << config.processing.lpr_batch_size << ",\"job_queue_capacity\":"
              << config.processing.job_queue_capacity
              << ",\"queue_overflow\":\""
              << deepstream_runtime::queue_overflow_name(
                     config.processing.queue_overflow)
              << "\"}}\n";
    return 0;
  }
  if (print_graph) {
    std::cout << deepstream_runtime::pipeline_description(config) << '\n';
    return 0;
  }
  deepstream_runtime::PipelineRunner runner(std::move(config));
  const int result = runner.run(&error);
  if (result != 0 && !error.empty()) std::cerr << error << '\n';
  return result;
}
