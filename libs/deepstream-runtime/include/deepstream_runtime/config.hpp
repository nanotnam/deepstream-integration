#pragma once

#include "alpr/types.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace deepstream_runtime {

enum class SourceType { kFile, kRtsp };
enum class Precision { kFp16, kInt8 };

struct SourceConfig {
  std::string id;
  SourceType type{SourceType::kFile};
  std::string uri;
  std::string uri_env;
  uint32_t width{1920};
  uint32_t height{1080};
  double framerate{30.0};
  uint32_t latency_ms{200};
};

struct ModelConfig {
  std::string bundle;
  Precision precision{Precision::kFp16};
  std::filesystem::path engine_root{".local/engines"};
};

struct TrackerConfig {
  std::string type{"nvdcf"};
  std::filesystem::path config;
};

enum class QueueOverflow { kBlock };

struct ProcessingConfig {
  double max_fps{0.0};
  size_t plate_batch_size{1};
  size_t lpr_batch_size{1};
  size_t job_queue_capacity{256};
  QueueOverflow queue_overflow{QueueOverflow::kBlock};
};

struct KafkaConfig {
  bool enabled{true};
  std::string brokers_env{"ALPR_KAFKA_BROKERS"};
  std::string event_topic{"mbfs.alpr.events.v1"};
  std::string health_topic{"mbfs.alpr.health.v1"};
};

struct OutputConfig {
  bool stdout_enabled{true};
  KafkaConfig kafka;
};

struct HealthConfig {
  uint32_t interval_seconds{5};
};

struct PipelineConfig {
  std::string schema;
  SourceConfig source;
  ModelConfig models;
  TrackerConfig tracker;
  ProcessingConfig processing;
  alpr::Settings alpr;
  OutputConfig outputs;
  HealthConfig health;
};

using EnvironmentLookup = std::function<const char*(const char*)>;

bool load_pipeline_config(const std::filesystem::path& path,
                          PipelineConfig* config, std::string* error);
bool validate_pipeline_config(const PipelineConfig& config, std::string* error);
bool resolve_source_uri(const PipelineConfig& config, const EnvironmentLookup& lookup,
                        std::string* uri, std::string* error);
std::string redact_uri(const std::string& uri);
std::string precision_name(Precision precision);
std::string source_type_name(SourceType source_type);
std::string queue_overflow_name(QueueOverflow overflow);

}  // namespace deepstream_runtime
