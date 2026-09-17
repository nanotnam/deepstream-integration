#pragma once

#include "deepstream_runtime/config.hpp"

#include <string>

namespace deepstream_runtime {

std::string pipeline_description(const PipelineConfig& config);
bool deepstream_runtime_compiled();

}  // namespace deepstream_runtime

