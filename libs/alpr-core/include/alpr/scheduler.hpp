#pragma once

#include "alpr/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace alpr {

bool point_in_polygon(const Point& point, const std::vector<Point>& polygon);
bool vehicle_in_zone(const Detection& vehicle, const std::vector<Point>& zone);

class EveryVehiclePlanner {
 public:
  explicit EveryVehiclePlanner(std::vector<Point> recognition_zone);
  std::vector<PlateJob> plan(const std::string& source_id, uint64_t frame_number,
                             uint64_t frame_pts_ns,
                             const std::vector<TrackedVehicle>& tracks) const;
  void update_zone(std::vector<Point> recognition_zone);

 private:
  std::vector<Point> recognition_zone_;
};

std::vector<PlateBatch> split_plate_batches(std::vector<PlateJob> jobs,
                                             size_t batch_size);

class FrameRateGate {
 public:
  explicit FrameRateGate(double maximum_fps = 0.0);
  bool accept(uint64_t frame_pts_ns);
  void reset(double maximum_fps);

 private:
  uint64_t interval_ns_{0};
  uint64_t last_accepted_pts_ns_{0};
  bool has_accepted_{false};
};

}  // namespace alpr
