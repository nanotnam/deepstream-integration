#include "motorcycle_violation/config.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace motorcycle_violation {
namespace {

std::string trim(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
    return std::isspace(c) != 0;
  });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
    return std::isspace(c) != 0;
  }).base();
  if (first >= last) return {};
  return std::string(first, last);
}

std::string strip_comment(const std::string& line) {
  char quote = '\0';
  for (size_t index = 0; index < line.size(); ++index) {
    const char character = line[index];
    if ((character == '\'' || character == '"') &&
        (index == 0U || line[index - 1U] != '\\')) {
      quote = quote == '\0' ? character : (quote == character ? '\0' : quote);
    }
    if (character == '#' && quote == '\0' &&
        (index == 0U || std::isspace(static_cast<unsigned char>(line[index - 1U])) != 0)) {
      return line.substr(0U, index);
    }
  }
  return line;
}

bool scalar(std::string value, std::string* output, std::string* error) {
  value = trim(std::move(value));
  if (value.empty()) {
    *error = "empty scalar value";
    return false;
  }
  if (value.front() == '\'' || value.front() == '"') {
    if (value.size() < 2U || value.back() != value.front()) {
      *error = "unterminated quoted scalar";
      return false;
    }
    value = value.substr(1U, value.size() - 2U);
  }
  *output = std::move(value);
  return true;
}

bool parse_document(const std::filesystem::path& path,
                    std::map<std::string, std::string>* values,
                    std::string* error) {
  std::ifstream input(path);
  if (!input) {
    *error = "cannot open pipeline configuration: " + path.string();
    return false;
  }
  std::vector<std::pair<size_t, std::string>> parents;
  std::string line;
  size_t line_number = 0U;
  while (std::getline(input, line)) {
    ++line_number;
    line = strip_comment(line);
    if (trim(line).empty()) continue;
    if (line.find('\t') != std::string::npos) {
      *error = "tabs are not allowed at line " + std::to_string(line_number);
      return false;
    }
    const size_t indent = line.find_first_not_of(' ');
    if (indent == std::string::npos || indent % 2U != 0U) {
      *error = "indentation must use two spaces at line " + std::to_string(line_number);
      return false;
    }
    const std::string body = line.substr(indent);
    const size_t colon = body.find(':');
    if (colon == std::string::npos) {
      *error = "expected key:value mapping at line " + std::to_string(line_number);
      return false;
    }
    const std::string key = trim(body.substr(0U, colon));
    if (key.empty() || key.front() == '-') {
      *error = "block sequences and empty keys are unsupported at line " +
               std::to_string(line_number);
      return false;
    }
    while (!parents.empty() && parents.back().first >= indent) parents.pop_back();
    std::string path_key;
    for (const auto& [parent_indent, parent] : parents) {
      static_cast<void>(parent_indent);
      if (!path_key.empty()) path_key.push_back('.');
      path_key += parent;
    }
    if (!path_key.empty()) path_key.push_back('.');
    path_key += key;
    const std::string raw_value = trim(body.substr(colon + 1U));
    if (raw_value.empty()) {
      parents.emplace_back(indent, key);
      continue;
    }
    std::string value;
    if (!scalar(raw_value, &value, error)) {
      *error += " at line " + std::to_string(line_number);
      return false;
    }
    if (!values->emplace(path_key, value).second) {
      *error = "duplicate key '" + path_key + "'";
      return false;
    }
  }
  return true;
}

template <typename Integer>
bool integer_value(const std::string& value, Integer* output) {
  const char* begin = value.data();
  const char* end = begin + value.size();
  const auto result = std::from_chars(begin, end, *output);
  return result.ec == std::errc{} && result.ptr == end;
}

bool float_value(const std::string& value, float* output) {
  try {
    size_t consumed = 0U;
    *output = std::stof(value, &consumed);
    return consumed == value.size();
  } catch (const std::exception&) {
    return false;
  }
}

bool double_value(const std::string& value, double* output) {
  try {
    size_t consumed = 0U;
    *output = std::stod(value, &consumed);
    return consumed == value.size();
  } catch (const std::exception&) {
    return false;
  }
}

bool bool_value(const std::string& value, bool* output) {
  if (value == "true") {
    *output = true;
    return true;
  }
  if (value == "false") {
    *output = false;
    return true;
  }
  return false;
}

bool zone_value(std::string value, std::vector<alpr::Point>* points) {
  for (char& character : value) {
    if (character == '[' || character == ']' || character == ',') character = ' ';
  }
  std::istringstream stream(value);
  std::vector<float> coordinates;
  float coordinate = 0.0F;
  while (stream >> coordinate) coordinates.push_back(coordinate);
  if (!stream.eof() || coordinates.size() < 6U || coordinates.size() % 2U != 0U) {
    return false;
  }
  points->clear();
  for (size_t index = 0; index < coordinates.size(); index += 2U) {
    points->push_back({coordinates[index], coordinates[index + 1U]});
  }
  return true;
}

bool set_values(const std::map<std::string, std::string>& values,
                PipelineConfig* config, std::string* error) {
  const std::set<std::string> known{
      "schema", "source.id", "source.type", "source.uri", "source.uri_env",
      "source.width", "source.height", "source.framerate", "source.latency_ms",
      "source.reconnect_initial_ms", "source.reconnect_maximum_ms",
      "models.bundle", "models.precision", "models.engine_root", "tracker.type",
      "tracker.config", "tracker.exit_grace_frames", "processing.max_fps", "processing.plate_batch_size",
      "processing.lpr_batch_size", "processing.job_queue_capacity",
      "processing.queue_overflow", "alpr.vehicle_threshold", "alpr.plate_threshold",
      "alpr.vehicle_iou_threshold", "alpr.plate_iou_threshold",
      "alpr.minimum_finalize_observations", "alpr.maximum_values_per_index",
      "alpr.character_lock_confidence", "alpr.recognition_zone",
      "outputs.stdout.enabled", "outputs.kafka.enabled", "outputs.kafka.brokers_env",
      "outputs.kafka.adapter_config",
      "outputs.kafka.event_topic", "outputs.kafka.health_topic",
      "outputs.kafka.queue_capacity", "health.interval_seconds",
      "runtime.shutdown_timeout_seconds"};
  for (const auto& [key, value] : values) {
    static_cast<void>(value);
    if (known.count(key) == 0U) {
      *error = "unknown configuration key '" + key + "'";
      return false;
    }
  }
  const auto get = [&](const char* key) -> const std::string* {
    const auto item = values.find(key);
    return item == values.end() ? nullptr : &item->second;
  };
  if (const auto* value = get("schema")) config->schema = *value;
  if (const auto* value = get("source.id")) config->source.id = *value;
  if (const auto* value = get("source.type")) {
    if (*value == "file") config->source.type = SourceType::kFile;
    else if (*value == "rtsp") config->source.type = SourceType::kRtsp;
    else {
      *error = "source.type must be file or rtsp";
      return false;
    }
  }
  if (const auto* value = get("source.uri")) config->source.uri = *value;
  if (const auto* value = get("source.uri_env")) config->source.uri_env = *value;
  if (const auto* value = get("source.width"); value && !integer_value(*value, &config->source.width)) {
    *error = "source.width must be an integer"; return false;
  }
  if (const auto* value = get("source.height"); value && !integer_value(*value, &config->source.height)) {
    *error = "source.height must be an integer"; return false;
  }
  if (const auto* value = get("source.framerate"); value && !double_value(*value, &config->source.framerate)) {
    *error = "source.framerate must be numeric"; return false;
  }
  if (const auto* value = get("source.latency_ms"); value && !integer_value(*value, &config->source.latency_ms)) {
    *error = "source.latency_ms must be an integer"; return false;
  }
  if (const auto* value = get("source.reconnect_initial_ms");
      value && !integer_value(*value, &config->source.reconnect_initial_ms)) {
    *error = "source.reconnect_initial_ms must be an integer"; return false;
  }
  if (const auto* value = get("source.reconnect_maximum_ms");
      value && !integer_value(*value, &config->source.reconnect_maximum_ms)) {
    *error = "source.reconnect_maximum_ms must be an integer"; return false;
  }
  if (const auto* value = get("models.bundle")) config->models.bundle = *value;
  if (const auto* value = get("models.precision")) {
    if (*value == "fp16") config->models.precision = Precision::kFp16;
    else if (*value == "int8") config->models.precision = Precision::kInt8;
    else { *error = "models.precision must be fp16 or int8"; return false; }
  }
  if (const auto* value = get("models.engine_root")) config->models.engine_root = *value;
  if (const auto* value = get("tracker.type")) config->tracker.type = *value;
  if (const auto* value = get("tracker.config")) config->tracker.config = *value;
  if (const auto* value = get("processing.max_fps");
      value && !double_value(*value, &config->processing.max_fps)) {
    *error = "processing.max_fps must be numeric"; return false;
  }
  if (const auto* value = get("processing.queue_overflow")) {
    if (*value != "block") {
      *error = "processing.queue_overflow must be block"; return false;
    }
    config->processing.queue_overflow = QueueOverflow::kBlock;
  }

#define SET_FLOAT(KEY, FIELD) \
  if (const auto* value = get(KEY); value && !float_value(*value, &FIELD)) { \
    *error = std::string(KEY) + " must be numeric"; return false; \
  }
#define SET_SIZE(KEY, FIELD) \
  if (const auto* value = get(KEY); value && !integer_value(*value, &FIELD)) { \
    *error = std::string(KEY) + " must be an integer"; return false; \
  }
  SET_FLOAT("alpr.vehicle_threshold", config->alpr.vehicle_threshold)
  SET_FLOAT("alpr.plate_threshold", config->alpr.plate_threshold)
  SET_FLOAT("alpr.vehicle_iou_threshold", config->alpr.vehicle_iou_threshold)
  SET_FLOAT("alpr.plate_iou_threshold", config->alpr.plate_iou_threshold)
  SET_SIZE("processing.plate_batch_size", config->processing.plate_batch_size)
  SET_SIZE("processing.lpr_batch_size", config->processing.lpr_batch_size)
  SET_SIZE("processing.job_queue_capacity", config->processing.job_queue_capacity)
  SET_SIZE("tracker.exit_grace_frames", config->tracker.exit_grace_frames)
  SET_SIZE("alpr.minimum_finalize_observations", config->alpr.minimum_finalize_observations)
  SET_SIZE("alpr.maximum_values_per_index", config->alpr.maximum_values_per_index)
  SET_FLOAT("alpr.character_lock_confidence", config->alpr.character_lock_confidence)
#undef SET_FLOAT
#undef SET_SIZE
  if (const auto* value = get("alpr.recognition_zone");
      value && !zone_value(*value, &config->alpr.recognition_zone)) {
    *error = "alpr.recognition_zone must be an inline list of at least three [x,y] points";
    return false;
  }
  if (const auto* value = get("outputs.stdout.enabled");
      value && !bool_value(*value, &config->outputs.stdout_enabled)) {
    *error = "outputs.stdout.enabled must be true or false"; return false;
  }
  if (const auto* value = get("outputs.kafka.enabled");
      value && !bool_value(*value, &config->outputs.kafka.enabled)) {
    *error = "outputs.kafka.enabled must be true or false"; return false;
  }
  if (const auto* value = get("outputs.kafka.brokers_env")) config->outputs.kafka.brokers_env = *value;
  if (const auto* value = get("outputs.kafka.adapter_config")) config->outputs.kafka.adapter_config = *value;
  if (const auto* value = get("outputs.kafka.event_topic")) config->outputs.kafka.event_topic = *value;
  if (const auto* value = get("outputs.kafka.health_topic")) config->outputs.kafka.health_topic = *value;
  if (const auto* value = get("outputs.kafka.queue_capacity");
      value && !integer_value(*value, &config->outputs.kafka.queue_capacity)) {
    *error = "outputs.kafka.queue_capacity must be an integer"; return false;
  }
  if (const auto* value = get("health.interval_seconds");
      value && !integer_value(*value, &config->health.interval_seconds)) {
    *error = "health.interval_seconds must be an integer"; return false;
  }
  if (const auto* value = get("runtime.shutdown_timeout_seconds");
      value && !integer_value(*value, &config->runtime.shutdown_timeout_seconds)) {
    *error = "runtime.shutdown_timeout_seconds must be an integer"; return false;
  }
  return true;
}

bool valid_topic(const std::string& value) {
  return !value.empty() && value.find_first_of(" \t\r\n") == std::string::npos;
}

}  // namespace

bool load_pipeline_config(const std::filesystem::path& path,
                          PipelineConfig* config, std::string* error) {
  std::map<std::string, std::string> values;
  if (!parse_document(path, &values, error)) return false;
  PipelineConfig loaded;
  if (!set_values(values, &loaded, error) || !validate_pipeline_config(loaded, error)) {
    return false;
  }
  *config = std::move(loaded);
  return true;
}

bool validate_pipeline_config(const PipelineConfig& config, std::string* error) {
  if (config.schema != "mbfs.deepstream-pipeline/v1") {
    *error = "schema must be mbfs.deepstream-pipeline/v1";
    return false;
  }
  const bool valid_source_id = !config.source.id.empty() &&
      std::all_of(config.source.id.begin(), config.source.id.end(), [](unsigned char value) {
        return std::isalnum(value) != 0 || value == '-' || value == '_' || value == '.';
      });
  if (!valid_source_id) {
    *error = "source.id must be a non-empty token";
    return false;
  }
  if (config.source.uri.empty() == config.source.uri_env.empty()) {
    *error = "exactly one of source.uri and source.uri_env is required";
    return false;
  }
  if (config.source.width == 0U || config.source.height == 0U ||
      config.source.framerate <= 0.0 || config.source.framerate > 240.0) {
    *error = "source dimensions and framerate are invalid";
    return false;
  }
  if (config.source.reconnect_initial_ms == 0U ||
      config.source.reconnect_maximum_ms < config.source.reconnect_initial_ms) {
    *error = "source reconnect bounds are invalid";
    return false;
  }
  if (config.models.bundle.empty() || config.models.engine_root.empty()) {
    *error = "models.bundle and models.engine_root are required";
    return false;
  }
  if (config.tracker.type != "nvdcf" || config.tracker.config.empty()) {
    *error = "tracker.type must be nvdcf and tracker.config is required";
    return false;
  }
  if (config.tracker.exit_grace_frames == 0U) {
    *error = "tracker.exit_grace_frames must be positive";
    return false;
  }
  if (!std::isfinite(config.processing.max_fps) || config.processing.max_fps < 0.0 ||
      config.processing.max_fps > 240.0) {
    *error = "processing.max_fps must be in [0,240]";
    return false;
  }
  if (config.processing.plate_batch_size == 0U ||
      config.processing.lpr_batch_size == 0U ||
      config.processing.job_queue_capacity == 0U) {
    *error = "processing batch sizes and queue capacity must be positive";
    return false;
  }
  if (config.processing.job_queue_capacity < config.processing.plate_batch_size ||
      config.processing.job_queue_capacity < config.processing.lpr_batch_size) {
    *error = "processing.job_queue_capacity must cover both batch sizes";
    return false;
  }
  const auto probability = [](float value) { return value >= 0.0F && value <= 1.0F; };
  if (!probability(config.alpr.vehicle_threshold) ||
      !probability(config.alpr.plate_threshold) ||
      !probability(config.alpr.vehicle_iou_threshold) ||
      !probability(config.alpr.plate_iou_threshold) ||
      !probability(config.alpr.character_lock_confidence)) {
    *error = "ALPR thresholds must be in [0,1]";
    return false;
  }
  if (config.alpr.minimum_finalize_observations == 0U ||
      config.alpr.maximum_values_per_index == 0U ||
      config.alpr.recognition_zone.size() < 3U) {
    *error = "ALPR observation counts and recognition zone must be non-zero";
    return false;
  }
  for (const alpr::Point& point : config.alpr.recognition_zone) {
    if (point.x < 0.0F || point.x > 1.0F || point.y < 0.0F || point.y > 1.0F) {
      *error = "recognition zone coordinates must be normalized to [0,1]";
      return false;
    }
  }
  if (!config.outputs.stdout_enabled && !config.outputs.kafka.enabled) {
    *error = "at least one output must be enabled";
    return false;
  }
  if (config.outputs.kafka.enabled &&
      (config.outputs.kafka.brokers_env.empty() ||
       config.outputs.kafka.adapter_config.empty() ||
       !valid_topic(config.outputs.kafka.event_topic) ||
       !valid_topic(config.outputs.kafka.health_topic) ||
       config.outputs.kafka.queue_capacity == 0U)) {
    *error = "enabled Kafka output requires broker environment, adapter config, and valid topics";
    return false;
  }
  if (config.health.interval_seconds == 0U) {
    *error = "health.interval_seconds must be positive";
    return false;
  }
  if (config.runtime.shutdown_timeout_seconds == 0U) {
    *error = "runtime.shutdown_timeout_seconds must be positive";
    return false;
  }
  return true;
}

bool resolve_source_uri(const PipelineConfig& config, const EnvironmentLookup& lookup,
                        std::string* uri, std::string* error) {
  if (!config.source.uri.empty()) {
    *uri = config.source.uri;
    return true;
  }
  const char* value = lookup(config.source.uri_env.c_str());
  if (value == nullptr || *value == '\0') {
    *error = "source URI environment variable is unset: " + config.source.uri_env;
    return false;
  }
  *uri = value;
  return true;
}

std::string redact_uri(const std::string& uri) {
  const size_t scheme = uri.find("://");
  if (scheme == std::string::npos) return uri;
  const size_t at = uri.find('@', scheme + 3U);
  if (at == std::string::npos) return uri;
  return uri.substr(0U, scheme + 3U) + "<redacted>@" + uri.substr(at + 1U);
}

std::string precision_name(Precision precision) {
  return precision == Precision::kFp16 ? "fp16" : "int8";
}

std::string source_type_name(SourceType source_type) {
  return source_type == SourceType::kFile ? "file" : "rtsp";
}

std::string queue_overflow_name(QueueOverflow overflow) {
  static_cast<void>(overflow);
  return "block";
}

}  // namespace motorcycle_violation
