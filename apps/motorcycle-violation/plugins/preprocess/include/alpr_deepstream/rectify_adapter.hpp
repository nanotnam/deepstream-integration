#pragma once

#include "alpr/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

extern "C" bool MbfsAlprRectifyPackedImage(
    const uint8_t* source, size_t source_size, uint32_t width, uint32_t height,
    size_t stride, bool source_is_bgr, const alpr::Box* box,
    const std::array<alpr::Point, 5>* keypoints, uint8_t* destination_bgr,
    size_t destination_size);
