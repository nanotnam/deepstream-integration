#pragma once

#include "traffic_alpr/processing.hpp"

#include <gst/gst.h>

#include <cstdint>

extern "C" bool MbfsAlprSubmitEveryVehicleFrame(
    traffic_alpr::ProcessingCoordinator* coordinator, GstBuffer* buffer,
    uint32_t vehicle_component_id);
