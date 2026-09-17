#include "alpr/detection.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace alpr {
namespace {

struct Candidate {
  Detection detection;
  std::array<Point, 5> keypoints{};
};

float sigmoid(float value) {
  if (value >= 0.0F) return 1.0F / (1.0F + std::exp(-value));
  const float exponent = std::exp(value);
  return exponent / (1.0F + exponent);
}

Detection decoded_box(float center_x, float center_y,
                      const std::array<float, 4>& distance, float stride,
                      uint32_t source_width, uint32_t source_height,
                      const ImageTransform& transform) {
  const float scale_x = std::max(transform.scale_x, std::numeric_limits<float>::epsilon());
  const float scale_y = std::max(transform.scale_y, std::numeric_limits<float>::epsilon());
  float left = (center_x - distance[0] * stride - transform.pad_x) / scale_x;
  float top = (center_y - distance[1] * stride - transform.pad_y) / scale_y;
  float right = (center_x + distance[2] * stride - transform.pad_x) / scale_x;
  float bottom = (center_y + distance[3] * stride - transform.pad_y) / scale_y;
  left = std::clamp(left, 0.0F, static_cast<float>(source_width));
  top = std::clamp(top, 0.0F, static_cast<float>(source_height));
  right = std::clamp(right, 0.0F, static_cast<float>(source_width));
  bottom = std::clamp(bottom, 0.0F, static_cast<float>(source_height));
  Detection result;
  result.box.x = left / source_width;
  result.box.y = top / source_height;
  result.box.width = right / source_width - result.box.x;
  result.box.height = bottom / source_height - result.box.y;
  return result;
}

Point decoded_point(float center_x, float center_y, float dx, float dy, float stride,
                    uint32_t source_width, uint32_t source_height,
                    const ImageTransform& transform) {
  const float x = (center_x + dx * stride - transform.pad_x) /
                  std::max(transform.scale_x, std::numeric_limits<float>::epsilon());
  const float y = (center_y + dy * stride - transform.pad_y) /
                  std::max(transform.scale_y, std::numeric_limits<float>::epsilon());
  return {std::clamp(x / source_width, 0.0F, 1.0F),
          std::clamp(y / source_height, 0.0F, 1.0F)};
}

std::vector<Candidate> suppress(std::vector<Candidate> candidates, float threshold,
                                bool class_aware) {
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate& lhs, const Candidate& rhs) {
                     return lhs.detection.score > rhs.detection.score;
                   });
  std::vector<Candidate> kept;
  for (Candidate& candidate : candidates) {
    bool overlaps = false;
    for (const Candidate& existing : kept) {
      if ((!class_aware || candidate.detection.class_id == existing.detection.class_id) &&
          intersection_over_union(candidate.detection.box, existing.detection.box) > threshold) {
        overlaps = true;
        break;
      }
    }
    if (!overlaps) kept.push_back(std::move(candidate));
  }
  return kept;
}

const TensorView* matching_output(const std::vector<TensorView>& outputs,
                                  const std::vector<int64_t>& shape) {
  for (const TensorView& output : outputs) {
    if (output.shape == shape) return &output;
  }
  return nullptr;
}

bool valid_decode_context(uint32_t source_width, uint32_t source_height,
                          const ImageTransform& transform, std::string* error) {
  if (source_width == 0 || source_height == 0 || transform.target_width == 0 ||
      transform.target_height == 0 || transform.scale_x <= 0.0F ||
      transform.scale_y <= 0.0F) {
    *error = "decoder requires valid source and preprocessing geometry";
    return false;
  }
  return true;
}

}  // namespace

float intersection_over_union(const Box& first, const Box& second) {
  const float left = std::max(first.x, second.x);
  const float top = std::max(first.y, second.y);
  const float right = std::min(first.x + first.width, second.x + second.width);
  const float bottom = std::min(first.y + first.height, second.y + second.height);
  const float intersection = std::max(0.0F, right - left) *
                             std::max(0.0F, bottom - top);
  const float combined = first.width * first.height + second.width * second.height -
                         intersection;
  return combined > 0.0F ? intersection / combined : 0.0F;
}

bool decode_vehicle_scrfd(uint32_t source_width, uint32_t source_height,
                          const ImageTransform& transform,
                          const std::vector<TensorView>& outputs,
                          const std::vector<std::string>& labels,
                          const Settings& settings,
                          std::vector<Detection>* detections,
                          std::string* error) {
  detections->clear();
  if (!valid_decode_context(source_width, source_height, transform, error)) return false;
  if (labels.size() != 7U || outputs.empty()) {
    *error = "vehicle decoder requires seven labels and output tensors";
    return false;
  }
  std::vector<Candidate> candidates;
  for (const TensorView& scores : outputs) {
    if (scores.shape.size() != 3U || scores.shape[0] != 1 || scores.shape[2] != 7) continue;
    size_t score_count = 0;
    if (!validate_tensor(scores, &score_count, error)) return false;
    const size_t anchors = static_cast<size_t>(scores.shape[1]);
    const size_t cells = anchors / 3U;
    const size_t grid = static_cast<size_t>(std::llround(std::sqrt(cells)));
    if (grid == 0U || grid * grid * 3U != anchors) continue;
    const TensorView* boxes = matching_output(outputs, {1, scores.shape[1], 4});
    if (boxes == nullptr) {
      *error = "vehicle score output has no matching box output";
      return false;
    }
    size_t box_count = 0;
    if (!validate_tensor(*boxes, &box_count, error)) return false;
    static_cast<void>(score_count);
    static_cast<void>(box_count);
    const float stride = static_cast<float>(transform.target_width) / grid;
    for (size_t anchor = 0; anchor < anchors; ++anchor) {
      int32_t class_id = 0;
      float best = tensor_value(scores, anchor * 7U);
      for (int32_t class_index = 1; class_index < 7; ++class_index) {
        const float score = tensor_value(scores, anchor * 7U + class_index);
        if (score > best) {
          best = score;
          class_id = class_index;
        }
      }
      if (best < settings.vehicle_threshold || class_id == 4) continue;
      const size_t cell = anchor / 3U;
      std::array<float, 4> distances{};
      for (size_t index = 0; index < distances.size(); ++index) {
        distances[index] = tensor_value(*boxes, anchor * 4U + index);
      }
      Candidate candidate;
      candidate.detection = decoded_box(
          static_cast<float>(cell % grid) * stride,
          static_cast<float>(cell / grid) * stride, distances, stride,
          source_width, source_height, transform);
      if (candidate.detection.box.width <= 0.0F ||
          candidate.detection.box.height <= 0.0F) continue;
      candidate.detection.class_id = class_id;
      candidate.detection.label = labels[static_cast<size_t>(class_id)];
      candidate.detection.score = best;
      if (candidates.size() == 8192U) {
        *error = "vehicle candidate capacity exceeded";
        return false;
      }
      candidates.push_back(std::move(candidate));
    }
  }
  constexpr size_t kPreNmsLimit = 1024;
  if (candidates.size() > kPreNmsLimit) {
    std::partial_sort(candidates.begin(), candidates.begin() + kPreNmsLimit,
                      candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
                        return lhs.detection.score > rhs.detection.score;
                      });
    candidates.resize(kPreNmsLimit);
  }
  candidates = suppress(std::move(candidates), settings.vehicle_iou_threshold, true);
  if (candidates.size() > 300U) candidates.resize(300U);
  detections->reserve(candidates.size());
  for (Candidate& candidate : candidates) {
    detections->push_back(std::move(candidate.detection));
  }
  return true;
}

bool decode_plate_scrfd(uint32_t source_width, uint32_t source_height,
                        const ImageTransform& transform,
                        const std::vector<TensorView>& outputs,
                        const Settings& settings, PlateDetection* plate,
                        bool* found, std::string* error) {
  *found = false;
  if (!valid_decode_context(source_width, source_height, transform, error)) return false;
  if (outputs.empty()) {
    *error = "plate decoder requires output tensors";
    return false;
  }
  std::vector<Candidate> candidates;
  for (const TensorView& scores : outputs) {
    if (scores.shape.size() != 4U || scores.shape[0] != 1 ||
        (scores.shape[1] != 2 && scores.shape[3] != 2)) continue;
    size_t score_count = 0;
    if (!validate_tensor(scores, &score_count, error)) return false;
    const bool channel_first = scores.shape[1] == 2;
    const int64_t height = channel_first ? scores.shape[2] : scores.shape[1];
    const int64_t width = channel_first ? scores.shape[3] : scores.shape[2];
    const TensorView* boxes = matching_output(
        outputs, channel_first ? std::vector<int64_t>{1, 8, height, width}
                               : std::vector<int64_t>{1, height, width, 8});
    const TensorView* keypoints = matching_output(
        outputs, channel_first ? std::vector<int64_t>{1, 20, height, width}
                               : std::vector<int64_t>{1, height, width, 20});
    if (boxes == nullptr || keypoints == nullptr) {
      *error = "plate score output has no matching box/keypoint outputs";
      return false;
    }
    size_t ignored = 0;
    if (!validate_tensor(*boxes, &ignored, error) ||
        !validate_tensor(*keypoints, &ignored, error)) return false;
    static_cast<void>(score_count);
    const float stride = static_cast<float>(transform.target_width) /
                         static_cast<float>(width);
    const size_t cells = static_cast<size_t>(height * width);
    for (size_t cell = 0; cell < cells; ++cell) {
      for (size_t anchor = 0; anchor < 2U; ++anchor) {
        const size_t score_offset = channel_first ? anchor * cells + cell
                                                  : cell * 2U + anchor;
        const float score = sigmoid(tensor_value(scores, score_offset));
        if (score < settings.plate_threshold) continue;
        const float center_x = static_cast<float>(cell % static_cast<size_t>(width)) * stride;
        const float center_y = static_cast<float>(cell / static_cast<size_t>(width)) * stride;
        std::array<float, 4> distances{};
        for (size_t index = 0; index < distances.size(); ++index) {
          const size_t channel = anchor * 4U + index;
          const size_t offset = channel_first ? channel * cells + cell
                                              : cell * 8U + channel;
          distances[index] = tensor_value(*boxes, offset);
        }
        Candidate candidate;
        candidate.detection = decoded_box(center_x, center_y, distances, stride,
                                          source_width, source_height, transform);
        if (candidate.detection.box.width <= 0.0F ||
            candidate.detection.box.height <= 0.0F) continue;
        candidate.detection.class_id = 0;
        candidate.detection.label = "plate";
        candidate.detection.score = score;
        for (size_t point = 0; point < candidate.keypoints.size(); ++point) {
          const size_t channel = anchor * 10U + point * 2U;
          const size_t offset = channel_first ? channel * cells + cell
                                              : cell * 20U + channel;
          const size_t next_offset = channel_first ? (channel + 1U) * cells + cell
                                                   : offset + 1U;
          candidate.keypoints[point] = decoded_point(
              center_x, center_y, tensor_value(*keypoints, offset),
              tensor_value(*keypoints, next_offset), stride,
              source_width, source_height, transform);
        }
        if (candidates.size() == 4096U) {
          *error = "plate candidate capacity exceeded";
          return false;
        }
        candidates.push_back(std::move(candidate));
      }
    }
  }
  if (candidates.empty()) return true;
  candidates = suppress(std::move(candidates), settings.plate_iou_threshold, false);
  plate->detection = candidates.front().detection;
  plate->keypoints = candidates.front().keypoints;
  *found = true;
  return true;
}

void map_plate_to_source(const Box& region, PlateDetection* plate) {
  Box& box = plate->detection.box;
  box.x = region.x + box.x * region.width;
  box.y = region.y + box.y * region.height;
  box.width *= region.width;
  box.height *= region.height;
  for (Point& point : plate->keypoints) {
    point.x = region.x + point.x * region.width;
    point.y = region.y + point.y * region.height;
  }
}

}  // namespace alpr
