#include "deepstream_runtime/engine_set.hpp"

#include <openssl/evp.h>

#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

namespace deepstream_runtime {
namespace {

std::optional<size_t> value_position(const std::string& json, std::string_view key) {
  const std::string marker = "\"" + std::string(key) + "\"";
  const size_t key_position = json.find(marker);
  if (key_position == std::string::npos) return std::nullopt;
  const size_t colon = json.find(':', key_position + marker.size());
  if (colon == std::string::npos) return std::nullopt;
  size_t position = colon + 1U;
  while (position < json.size() &&
         std::isspace(static_cast<unsigned char>(json[position])) != 0) ++position;
  return position;
}

bool json_string(const std::string& json, std::string_view key, std::string* value) {
  const auto position = value_position(json, key);
  if (!position || *position >= json.size() || json[*position] != '"') return false;
  value->clear();
  bool escaped = false;
  for (size_t index = *position + 1U; index < json.size(); ++index) {
    const char character = json[index];
    if (escaped) {
      if (character != '"' && character != '\\' && character != '/') return false;
      value->push_back(character);
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == '"') {
      return true;
    } else {
      value->push_back(character);
    }
  }
  return false;
}

bool json_unsigned(const std::string& json, std::string_view key, size_t* value) {
  const auto position = value_position(json, key);
  if (!position) return false;
  size_t end = *position;
  while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end])) != 0) {
    ++end;
  }
  if (end == *position) return false;
  try {
    *value = static_cast<size_t>(std::stoull(json.substr(*position, end - *position)));
  } catch (...) {
    return false;
  }
  return true;
}

bool json_bool(const std::string& json, std::string_view key, bool* value) {
  const auto position = value_position(json, key);
  if (!position) return false;
  if (json.compare(*position, 4U, "true") == 0) {
    *value = true;
    return true;
  }
  if (json.compare(*position, 5U, "false") == 0) {
    *value = false;
    return true;
  }
  return false;
}

bool json_object(const std::string& json, std::string_view key, std::string* value) {
  const auto position = value_position(json, key);
  if (!position || *position >= json.size() || json[*position] != '{') return false;
  size_t depth = 0U;
  bool quoted = false;
  bool escaped = false;
  for (size_t index = *position; index < json.size(); ++index) {
    const char character = json[index];
    if (quoted) {
      if (escaped) escaped = false;
      else if (character == '\\') escaped = true;
      else if (character == '"') quoted = false;
      continue;
    }
    if (character == '"') quoted = true;
    else if (character == '{') ++depth;
    else if (character == '}' && --depth == 0U) {
      *value = json.substr(*position, index - *position + 1U);
      return true;
    }
  }
  return false;
}

bool read_file(const std::filesystem::path& path, std::string* value) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  std::ostringstream stream;
  stream << input.rdbuf();
  *value = stream.str();
  return input.good() || input.eof();
}

bool file_sha256(const std::filesystem::path& path, std::string* digest) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  EVP_MD_CTX* context = EVP_MD_CTX_new();
  if (context == nullptr) return false;
  bool success = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1;
  std::array<char, 1024U * 1024U> buffer{};
  while (success && input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = input.gcount();
    if (count > 0) {
      success = EVP_DigestUpdate(context, buffer.data(), static_cast<size_t>(count)) == 1;
    }
  }
  std::array<unsigned char, EVP_MAX_MD_SIZE> bytes{};
  unsigned int count = 0U;
  success = success && EVP_DigestFinal_ex(context, bytes.data(), &count) == 1;
  EVP_MD_CTX_free(context);
  if (!success) return false;
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (unsigned int index = 0U; index < count; ++index) {
    output << std::setw(2) << static_cast<unsigned>(bytes[index]);
  }
  *digest = output.str();
  return true;
}

bool parse_batch(const std::string& object, BatchProfile* batch) {
  return json_bool(object, "dynamic", &batch->dynamic) &&
         json_unsigned(object, "minimum", &batch->minimum) &&
         json_unsigned(object, "optimal", &batch->optimal) &&
         json_unsigned(object, "maximum", &batch->maximum);
}

bool same_runtime(const RuntimeIdentity& lhs, const RuntimeIdentity& rhs) {
  return lhs.deepstream == rhs.deepstream && lhs.cuda == rhs.cuda &&
         lhs.tensorrt == rhs.tensorrt && lhs.gpu == rhs.gpu &&
         lhs.gpu_compute_capability == rhs.gpu_compute_capability;
}

}  // namespace

bool load_engine_set(const std::filesystem::path& descriptor,
                     EngineSet* engine_set, std::string* error) {
  std::string json;
  if (!read_file(descriptor, &json)) {
    *error = "cannot read engine-set descriptor";
    return false;
  }
  std::string schema;
  size_t parser_abi = 0U;
  EngineSet parsed;
  parsed.descriptor = descriptor;
  if (!json_string(json, "schema", &schema) ||
      schema != "mbfs.tensorrt-engine-set/v1" ||
      !json_string(json, "bundle", &parsed.bundle) ||
      !json_string(json, "precision", &parsed.precision) ||
      !json_unsigned(json, "parser_abi", &parser_abi) ||
      !json_string(json, "deepstream", &parsed.runtime.deepstream) ||
      !json_string(json, "cuda", &parsed.runtime.cuda) ||
      !json_string(json, "tensorrt", &parsed.runtime.tensorrt) ||
      !json_string(json, "gpu", &parsed.runtime.gpu) ||
      !json_string(json, "gpu_compute_capability",
                   &parsed.runtime.gpu_compute_capability)) {
    *error = "engine-set descriptor is malformed or has an unsupported schema";
    return false;
  }
  parsed.parser_abi = static_cast<uint32_t>(parser_abi);
  std::string engines;
  std::string profiles;
  if (!json_object(json, "engines", &engines) ||
      !json_object(json, "batch_profiles", &profiles)) {
    *error = "engine-set descriptor is missing engines or batch profiles";
    return false;
  }
  for (const std::string role : {"vehicle", "plate", "lprnet"}) {
    std::string artifact_object;
    std::string profile_object;
    EngineArtifact artifact;
    std::string file;
    if (!json_object(engines, role, &artifact_object) ||
        !json_object(profiles, role, &profile_object) ||
        !json_string(artifact_object, "file", &file) ||
        !json_string(artifact_object, "sha256", &artifact.sha256) ||
        !json_string(artifact_object, "source_sha256", &artifact.source_sha256) ||
        !parse_batch(profile_object, &artifact.batch)) {
      *error = "engine-set descriptor has an invalid " + role + " entry";
      return false;
    }
    const std::filesystem::path relative(file);
    if (relative.empty() || relative.is_absolute() || relative.has_parent_path()) {
      *error = "engine-set contains an unsafe engine filename";
      return false;
    }
    artifact.path = std::filesystem::absolute(descriptor.parent_path() / relative);
    parsed.engines.emplace(role, std::move(artifact));
  }
  *engine_set = std::move(parsed);
  return true;
}

bool validate_engine_set(const EngineSet& engine_set, const std::string& bundle,
                         const std::string& precision, uint32_t parser_abi,
                         const RuntimeIdentity& runtime,
                         const ModelSourceHashes& source_hashes,
                         std::string* error) {
  if (engine_set.bundle != bundle || engine_set.precision != precision ||
      engine_set.parser_abi != parser_abi) {
    *error = "engine-set bundle, precision, or parser ABI does not match";
    return false;
  }
  if (!same_runtime(engine_set.runtime, runtime)) {
    *error = "engine-set runtime or GPU identity does not match";
    return false;
  }
  for (const std::string role : {"vehicle", "plate", "lprnet"}) {
    const auto artifact = engine_set.engines.find(role);
    const auto source = source_hashes.find(role);
    if (artifact == engine_set.engines.end() || source == source_hashes.end() ||
        artifact->second.source_sha256 != source->second) {
      *error = "engine-set source hash does not match for role " + role;
      return false;
    }
    std::string actual_hash;
    if (!file_sha256(artifact->second.path, &actual_hash) ||
        actual_hash != artifact->second.sha256) {
      *error = "engine file is missing or has the wrong hash for role " + role;
      return false;
    }
  }
  return true;
}

bool select_engine_set(const std::filesystem::path& engine_root,
                       const std::string& bundle, const std::string& precision,
                       uint32_t parser_abi, const RuntimeIdentity& runtime,
                       const ModelSourceHashes& source_hashes,
                       EngineSet* selected, std::string* error) {
  const std::filesystem::path bundle_root = engine_root / bundle;
  std::error_code filesystem_error;
  if (!std::filesystem::is_directory(bundle_root, filesystem_error)) {
    *error = "no engine sets exist for the configured bundle";
    return false;
  }
  std::vector<EngineSet> matches;
  for (const auto& entry : std::filesystem::directory_iterator(bundle_root, filesystem_error)) {
    if (filesystem_error) break;
    const auto descriptor = entry.path() / "engine-set.json";
    if (!entry.is_directory() || !std::filesystem::is_regular_file(descriptor)) continue;
    EngineSet candidate;
    std::string candidate_error;
    if (load_engine_set(descriptor, &candidate, &candidate_error) &&
        validate_engine_set(candidate, bundle, precision, parser_abi, runtime,
                            source_hashes, &candidate_error)) {
      matches.push_back(std::move(candidate));
    }
  }
  if (filesystem_error) {
    *error = "cannot enumerate engine sets";
    return false;
  }
  if (matches.empty()) {
    *error = "no compatible engine set was found";
    return false;
  }
  if (matches.size() != 1U) {
    *error = "multiple compatible engine sets were found";
    return false;
  }
  *selected = std::move(matches.front());
  return true;
}

}  // namespace deepstream_runtime
