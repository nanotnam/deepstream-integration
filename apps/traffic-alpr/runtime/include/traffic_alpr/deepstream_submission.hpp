#pragma once

#include "traffic_alpr/processing.hpp"

#include <gst/gst.h>

#include <cstdint>
#include <functional>

namespace traffic_alpr {

bool submit_deepstream_buffer(ProcessingCoordinator* coordinator, GstBuffer* buffer,
                              uint32_t vehicle_component_id,
                              const std::function<void(uint64_t, uint64_t)>& mark_seen = {},
                              const std::function<void(uint64_t)>& frame_done = {});

}  // namespace traffic_alpr
