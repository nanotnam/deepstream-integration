#pragma once

#include "alpr/types.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace alpr {

bool valid_image(const ImageView& image, std::string* error);
bool valid_plate_keypoints(const std::array<Point, 5>& keypoints);
bool is_two_line_plate(const std::array<Point, 5>& keypoints);
bool rectify_plate_bgr(const ImageView& image, const PlateDetection& plate,
                       std::vector<uint8_t>* output, std::string* error);

}  // namespace alpr

