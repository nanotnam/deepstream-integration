#include "alpr/scheduler.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace alpr {
namespace {

bool on_segment(const Point& point, const Point& first, const Point& second) {
  constexpr float kEpsilon = 1.0e-6F;
  const float cross = (point.y - first.y) * (second.x - first.x) -
                      (point.x - first.x) * (second.y - first.y);
  if (std::fabs(cross) > kEpsilon) return false;
  return point.x >= std::min(first.x, second.x) - kEpsilon &&
         point.x <= std::max(first.x, second.x) + kEpsilon &&
         point.y >= std::min(first.y, second.y) - kEpsilon &&
         point.y <= std::max(first.y, second.y) + kEpsilon;
}

}  // namespace

bool point_in_polygon(const Point& point, const std::vector<Point>& polygon) {
  if (polygon.size() < 3U) return false;
  bool inside = false;
  for (size_t current = 0U, previous = polygon.size() - 1U;
       current < polygon.size(); previous = current++) {
    const Point& first = polygon[previous];
    const Point& second = polygon[current];
    if (on_segment(point, first, second)) return true;
    const bool crosses = (first.y > point.y) != (second.y > point.y);
    if (crosses) {
      const float intersection_x = first.x +
          (point.y - first.y) * (second.x - first.x) / (second.y - first.y);
      if (intersection_x > point.x) inside = !inside;
    }
  }
  return inside;
}

bool vehicle_in_zone(const Detection& vehicle, const std::vector<Point>& zone) {
  const Point bottom_center{vehicle.box.x + vehicle.box.width * 0.5F,
                            vehicle.box.y + vehicle.box.height};
  return point_in_polygon(bottom_center, zone);
}

EveryVehiclePlanner::EveryVehiclePlanner(std::vector<Point> recognition_zone)
    : recognition_zone_(std::move(recognition_zone)) {}

std::vector<PlateJob> EveryVehiclePlanner::plan(
    const std::string& source_id, uint64_t frame_number, uint64_t frame_pts_ns,
    const std::vector<TrackedVehicle>& tracks) const {
  std::vector<PlateJob> jobs;
  jobs.reserve(tracks.size());
  for (const TrackedVehicle& track : tracks) {
    if (!track.active || !vehicle_in_zone(track.detection, recognition_zone_)) continue;
    jobs.push_back({source_id, frame_number, frame_pts_ns, track.track_id,
                    track.detection});
  }
  return jobs;
}

void EveryVehiclePlanner::update_zone(std::vector<Point> recognition_zone) {
  recognition_zone_ = std::move(recognition_zone);
}

std::vector<PlateBatch> split_plate_batches(std::vector<PlateJob> jobs,
                                             size_t batch_size) {
  if (batch_size == 0U) throw std::invalid_argument("batch size must be positive");
  std::vector<PlateBatch> batches;
  for (size_t begin = 0U; begin < jobs.size(); begin += batch_size) {
    const size_t end = std::min(begin + batch_size, jobs.size());
    PlateBatch batch;
    batch.jobs.reserve(end - begin);
    for (size_t index = begin; index < end; ++index) {
      batch.jobs.push_back(std::move(jobs[index]));
    }
    batches.push_back(std::move(batch));
  }
  return batches;
}

FrameRateGate::FrameRateGate(double maximum_fps) { reset(maximum_fps); }

bool FrameRateGate::accept(uint64_t frame_pts_ns) {
  if (interval_ns_ == 0U) return true;
  if (!has_accepted_ || frame_pts_ns < last_accepted_pts_ns_ ||
      frame_pts_ns - last_accepted_pts_ns_ >= interval_ns_) {
    has_accepted_ = true;
    last_accepted_pts_ns_ = frame_pts_ns;
    return true;
  }
  return false;
}

void FrameRateGate::reset(double maximum_fps) {
  if (!std::isfinite(maximum_fps) || maximum_fps < 0.0) {
    throw std::invalid_argument("maximum FPS must be finite and non-negative");
  }
  interval_ns_ = maximum_fps == 0.0
      ? 0U
      : static_cast<uint64_t>(std::llround(1000000000.0 / maximum_fps));
  last_accepted_pts_ns_ = 0U;
  has_accepted_ = false;
}

}  // namespace alpr
