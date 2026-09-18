#pragma once

#include "motorcycle_violation/config.hpp"

#include <string>

namespace motorcycle_violation {

std::string pipeline_description(const PipelineConfig& config);
bool motorcycle_violation_compiled();

}  // namespace motorcycle_violation
