#pragma once

#include "alpr/tensor.hpp"
#include "alpr/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace alpr {

float intersection_over_union(const Box& first, const Box& second);

bool decode_vehicle_scrfd(uint32_t source_width, uint32_t source_height,
                          const ImageTransform& transform,
                          const std::vector<TensorView>& outputs,
                          const std::vector<std::string>& labels,
                          const Settings& settings,
                          std::vector<Detection>* detections,
                          std::string* error);

bool decode_plate_scrfd(uint32_t source_width, uint32_t source_height,
                        const ImageTransform& transform,
                        const std::vector<TensorView>& outputs,
                        const Settings& settings, PlateDetection* plate,
                        bool* found, std::string* error);

struct PlateDecodeContext {
  uint32_t source_width{0};
  uint32_t source_height{0};
  ImageTransform transform;
};

bool decode_plate_scrfd_batch(
    const std::vector<PlateDecodeContext>& contexts,
    const std::vector<TensorView>& outputs, const Settings& settings,
    std::vector<std::optional<PlateDetection>>* plates, std::string* error);

void map_plate_to_source(const Box& region, PlateDetection* plate);

}  // namespace alpr
