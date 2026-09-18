#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace motorcycle_violation {

bool launch_plate_preprocess_rgba(const uint8_t* rgba, size_t pitch,
                                  uint32_t width, uint32_t height,
                                  float roi_x, float roi_y, float roi_width,
                                  float roi_height, float* output,
                                  void* stream);

bool launch_lpr_rectify_rgba(const uint8_t* rgba, size_t pitch,
                             uint32_t width, uint32_t height,
                             const std::array<double, 9>& transform,
                             bool two_line, float* output, void* stream);

}  // namespace motorcycle_violation
