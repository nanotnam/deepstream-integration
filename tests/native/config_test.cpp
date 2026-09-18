#include "deepstream_runtime/config.hpp"
#include "deepstream_runtime/engine_contract.hpp"
#include "deepstream_runtime/pipeline_description.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  const std::filesystem::path root(TEST_SOURCE_DIR);
  deepstream_runtime::PipelineConfig config;
  std::string error;
  assert(deepstream_runtime::load_pipeline_config(
      root / "apps/traffic-alpr/configs/file.yaml", &config, &error));
  assert(config.schema == "mbfs.deepstream-pipeline/v1");
  assert(config.source.type == deepstream_runtime::SourceType::kFile);
  assert(config.processing.max_fps == 0.0);
  assert(config.processing.plate_batch_size == 1U);
  assert(config.processing.lpr_batch_size == 1U);
  assert(config.processing.job_queue_capacity == 256U);
  assert(config.source.reconnect_initial_ms == 1000U);
  assert(config.source.reconnect_maximum_ms == 30000U);
  assert(config.tracker.exit_grace_frames == 150U);
  assert(config.outputs.kafka.queue_capacity == 1024U);
  assert(config.runtime.shutdown_timeout_seconds == 30U);
  assert(config.alpr.recognition_zone.size() == 4U);
  assert(deepstream_runtime::pipeline_description(config).find("nvinfer[plate,batch=1]") !=
         std::string::npos);
  assert(deepstream_runtime::pipeline_description(config).find("every-vehicle-roi") !=
         std::string::npos);
  assert(deepstream_runtime::queue_overflow_name(config.processing.queue_overflow) ==
         "block");

  std::string uri;
  assert(deepstream_runtime::resolve_source_uri(
      config, [](const char*) { return "file:///tmp/test.mp4"; }, &uri, &error));
  assert(uri == "file:///tmp/test.mp4");
  assert(deepstream_runtime::redact_uri("rtsp://user:pass@camera/live") ==
         "rtsp://<redacted>@camera/live");

  auto invalid_processing = config;
  invalid_processing.processing.plate_batch_size = 8U;
  invalid_processing.processing.job_queue_capacity = 4U;
  assert(!deepstream_runtime::validate_pipeline_config(invalid_processing, &error));
  assert(error.find("queue_capacity") != std::string::npos);
  invalid_processing = config;
  invalid_processing.processing.max_fps = -1.0;
  assert(!deepstream_runtime::validate_pipeline_config(invalid_processing, &error));
  assert(error.find("max_fps") != std::string::npos);

  const deepstream_runtime::EngineBatchProfiles static_profiles;
  assert(deepstream_runtime::validate_engine_batch_profiles(
      config, static_profiles, &error));
  auto oversized_batch = config;
  oversized_batch.processing.plate_batch_size = 2U;
  assert(!deepstream_runtime::validate_engine_batch_profiles(
      oversized_batch, static_profiles, &error));
  assert(error.find("plate engine profile") != std::string::npos);

  const auto invalid = std::filesystem::temp_directory_path() /
                       "deepstream-integration-invalid.yaml";
  {
    std::ofstream output(invalid);
    output << "schema: mbfs.deepstream-pipeline/v1\nunknown: value\n";
  }
  assert(!deepstream_runtime::load_pipeline_config(invalid, &config, &error));
  assert(error.find("unknown") != std::string::npos);
  std::filesystem::remove(invalid);

  const auto legacy = std::filesystem::temp_directory_path() /
                      "deepstream-integration-legacy.yaml";
  {
    std::ofstream output(legacy);
    output << "schema: mbfs.deepstream-pipeline/v1\n"
              "alpr:\n  max_plate_jobs_per_frame: 3\n";
  }
  assert(!deepstream_runtime::load_pipeline_config(legacy, &config, &error));
  assert(error.find("max_plate_jobs_per_frame") != std::string::npos);
  std::filesystem::remove(legacy);
  return 0;
}
