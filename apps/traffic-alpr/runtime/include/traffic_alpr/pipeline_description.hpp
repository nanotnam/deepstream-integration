#pragma once

#include "traffic_alpr/config.hpp"

#include <string>

namespace traffic_alpr {

std::string pipeline_description(const PipelineConfig& config);
bool traffic_alpr_compiled();

}  // namespace traffic_alpr
