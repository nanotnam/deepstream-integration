#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
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
  size_t max_plate_jobs_per_frame{3};
  size_t max_lpr_jobs_per_frame{2};
  size_t plate_cache_frames{12};
  size_t lpr_retry_frames{8};
  size_t minimum_finalize_observations{3};
  size_t maximum_values_per_index{10};
  float character_lock_confidence{0.9F};
  std::vector<Point> recognition_zone{{0.0F, 0.0F}, {1.0F, 0.0F},
                                      {1.0F, 1.0F}, {0.0F, 1.0F}};
};

struct TrackedVehicle {
  uint64_t track_id{0};
  Detection detection;
  bool eligible{false};
  bool newly_created{false};
  bool finalized{false};
  size_t waiting_frames{0};
  size_t last_plate_frame{0};
  size_t last_lpr_frame{0};
  std::optional<PlateDetection> plate;
};

struct WorkSelection {
  std::vector<uint64_t> plate_track_ids;
  std::vector<uint64_t> lpr_track_ids;
  size_t eligible{0};
  size_t deferred_plate{0};
  size_t deferred_lpr{0};
  size_t cached{0};
  size_t finalized_suppressed{0};
};

}  // namespace alpr

