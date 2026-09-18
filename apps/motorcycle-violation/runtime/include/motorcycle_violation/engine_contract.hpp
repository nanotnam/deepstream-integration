#pragma once

#include "motorcycle_violation/config.hpp"

#include <cstddef>
#include <string>

namespace motorcycle_violation {

struct BatchProfile {
  size_t minimum{1};
  size_t optimal{1};
  size_t maximum{1};
  bool dynamic{false};
};

struct EngineBatchProfiles {
  BatchProfile vehicle;
  BatchProfile plate;
  BatchProfile lprnet;
};

bool validate_engine_batch_profiles(const PipelineConfig& config,
                                    const EngineBatchProfiles& profiles,
                                    std::string* error);

}  // namespace motorcycle_violation
