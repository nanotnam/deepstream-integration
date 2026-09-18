#pragma once

#include <string_view>

namespace deepstream_platform {

// This library is deliberately domain-neutral. Application graph construction,
// model bindings, and metadata remain owned by each application.
std::string_view boundary_name();

}  // namespace deepstream_platform
