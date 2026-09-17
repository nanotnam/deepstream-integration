#pragma once

#include "deepstream_runtime/processing.hpp"

#include <gst/gst.h>

#include <cstdint>

extern "C" bool MbfsAlprSubmitEveryVehicleFrame(
    deepstream_runtime::ProcessingCoordinator* coordinator, GstBuffer* buffer,
    uint32_t vehicle_component_id);
