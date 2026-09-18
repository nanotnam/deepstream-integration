#include "motorcycle_violation/engine_set.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void write(const std::filesystem::path& path, const std::string& value) {
  std::ofstream output(path, std::ios::binary);
  output << value;
}

std::string descriptor(const std::string& vehicle_hash) {
  return std::string(R"({
  "schema":"mbfs.tensorrt-engine-set/v1",
  "bundle":"1.0.0",
  "parser_abi":1,
  "precision":"fp16",
  "deepstream":"9.1",
  "cuda":"13.0",
  "tensorrt":"10.0",
  "gpu":"RTX 3060",
  "gpu_compute_capability":"8.6",
  "batch_profiles":{
    "vehicle":{"dynamic":true,"minimum":1,"optimal":1,"maximum":1},
    "plate":{"dynamic":false,"minimum":1,"optimal":1,"maximum":1},
    "lprnet":{"dynamic":false,"minimum":1,"optimal":1,"maximum":1}
  },
  "engines":{
    "vehicle":{"file":"vehicle.engine","sha256":")") + vehicle_hash + R"(","source_sha256":"source-v"},
    "plate":{"file":"plate.engine","sha256":"3b56c3520006aaeb038c24862bcde877284bc1654e73bda292d90db943c4573d","source_sha256":"source-p"},
    "lprnet":{"file":"lprnet.engine","sha256":"a02052d68ad07172f64e9352e2c48f955d33f2b4debc4da4fb5c3ef78963f6ea","source_sha256":"source-l"}
  }
})";
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "alpr-engine-set-test";
  std::filesystem::remove_all(root);
  const auto directory = root / "1.0.0" / "test-key";
  std::filesystem::create_directories(directory);
  write(directory / "vehicle.engine", "vehicle");
  write(directory / "plate.engine", "plate");
  write(directory / "lprnet.engine", "lprnet");
  write(directory / "engine-set.json",
        descriptor("b404ed3c370c8c264e53b1867929db93e94012bb618bb263f514e32fd9e5bc29"));

  const motorcycle_violation::RuntimeIdentity runtime{
      "9.1", "13.0", "10.0", "RTX 3060", "8.6"};
  const motorcycle_violation::ModelSourceHashes sources{
      {"vehicle", "source-v"}, {"plate", "source-p"}, {"lprnet", "source-l"}};
  motorcycle_violation::EngineSet selected;
  std::string error;
  assert(motorcycle_violation::select_engine_set(
      root, "1.0.0", "fp16", 1U, runtime, sources, &selected, &error));
  assert(selected.engines.at("plate").batch.maximum == 1U);

  auto wrong_runtime = runtime;
  wrong_runtime.gpu_compute_capability = "9.0";
  assert(!motorcycle_violation::validate_engine_set(
      selected, "1.0.0", "fp16", 1U, wrong_runtime, sources, &error));
  assert(error.find("identity") != std::string::npos);

  write(directory / "vehicle.engine", "corrupt");
  assert(!motorcycle_violation::validate_engine_set(
      selected, "1.0.0", "fp16", 1U, runtime, sources, &error));
  assert(error.find("hash") != std::string::npos);
  std::filesystem::remove_all(root);
  return 0;
}
