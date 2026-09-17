#include "deepstream_runtime/config.hpp"
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
  assert(config.alpr.recognition_zone.size() == 4U);
  assert(deepstream_runtime::pipeline_description(config).find("nvinfer[plate]") !=
         std::string::npos);

  std::string uri;
  assert(deepstream_runtime::resolve_source_uri(
      config, [](const char*) { return "file:///tmp/test.mp4"; }, &uri, &error));
  assert(uri == "file:///tmp/test.mp4");
  assert(deepstream_runtime::redact_uri("rtsp://user:pass@camera/live") ==
         "rtsp://<redacted>@camera/live");

  const auto invalid = std::filesystem::temp_directory_path() /
                       "deepstream-integration-invalid.yaml";
  {
    std::ofstream output(invalid);
    output << "schema: mbfs.deepstream-pipeline/v1\nunknown: value\n";
  }
  assert(!deepstream_runtime::load_pipeline_config(invalid, &config, &error));
  assert(error.find("unknown") != std::string::npos);
  std::filesystem::remove(invalid);
  return 0;
}

