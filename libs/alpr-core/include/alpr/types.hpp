#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace alpr {

struct Point {
  float x{0.0F};
  float y{0.0F};
};

inline bool operator==(const Point& lhs, const Point& rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

struct Box {
  float x{0.0F};
  float y{0.0F};
  float width{0.0F};
  float height{0.0F};
};

struct Detection {
  int32_t class_id{0};
  std::string label;
  float score{0.0F};
  Box box;
};

struct PlateDetection {
  Detection detection;
  std::array<Point, 5> keypoints{};
};

struct OcrResult {
  std::string text;
  std::vector<float> character_confidences;
  float confidence{0.0F};
};

enum class PixelFormat { kRgb, kBgr };

struct ImageView {
  uint32_t width{0};
  uint32_t height{0};
  size_t stride{0};
  PixelFormat format{PixelFormat::kRgb};
  const uint8_t* data{nullptr};
  size_t size{0};
};

struct ImageTransform {
  float scale_x{1.0F};
  float scale_y{1.0F};
  float pad_x{0.0F};
  float pad_y{0.0F};
  uint32_t target_width{0};
  uint32_t target_height{0};
};

struct Settings {
  float vehicle_threshold{0.6F};
  float plate_threshold{0.3F};
  float vehicle_iou_threshold{0.3F};
  float plate_iou_threshold{0.4F};
  size_t minimum_finalize_observations{3};
  size_t maximum_values_per_index{10};
  float character_lock_confidence{0.9F};
  std::vector<Point> recognition_zone{{0.0F, 0.0F}, {1.0F, 0.0F},
                                      {1.0F, 1.0F}, {0.0F, 1.0F}};
};

struct TrackedVehicle {
  uint64_t track_id{0};
  Detection detection;
  bool active{true};
};

struct PlateJob {
  std::string source_id;
  uint64_t frame_number{0};
  uint64_t frame_pts_ns{0};
  uint64_t track_id{0};
  Detection vehicle;
};

struct PlateBatch {
  std::vector<PlateJob> jobs;
};

}  // namespace alpr
