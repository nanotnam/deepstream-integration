#pragma once

#include "motorcycle_violation/processing.hpp"

#include <gst/gst.h>

#include <cstdint>

extern "C" bool MbfsAlprSubmitEveryVehicleFrame(
    motorcycle_violation::ProcessingCoordinator* coordinator, GstBuffer* buffer,
    uint32_t vehicle_component_id);
