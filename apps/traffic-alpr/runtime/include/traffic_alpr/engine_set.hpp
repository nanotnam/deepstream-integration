#pragma once

#include "traffic_alpr/engine_contract.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

namespace traffic_alpr {

struct RuntimeIdentity {
  std::string deepstream;
  std::string cuda;
  std::string tensorrt;
  std::string gpu;
  std::string gpu_compute_capability;
};

struct EngineArtifact {
  std::filesystem::path path;
  std::string sha256;
  std::string source_sha256;
  BatchProfile batch;
};

struct EngineSet {
  std::filesystem::path descriptor;
  std::string bundle;
  std::string precision;
  uint32_t parser_abi{0};
  RuntimeIdentity runtime;
  std::map<std::string, EngineArtifact> engines;
};

using ModelSourceHashes = std::map<std::string, std::string>;

bool load_engine_set(const std::filesystem::path& descriptor,
                     EngineSet* engine_set, std::string* error);
bool validate_engine_set(const EngineSet& engine_set, const std::string& bundle,
                         const std::string& precision, uint32_t parser_abi,
                         const RuntimeIdentity& runtime,
                         const ModelSourceHashes& source_hashes,
                         std::string* error);
bool select_engine_set(const std::filesystem::path& engine_root,
                       const std::string& bundle, const std::string& precision,
                       uint32_t parser_abi, const RuntimeIdentity& runtime,
                       const ModelSourceHashes& source_hashes,
                       EngineSet* selected, std::string* error);

}  // namespace traffic_alpr
