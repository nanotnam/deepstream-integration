#pragma once

#include "alpr/types.hpp"

#include <cstddef>
#include <vector>

namespace alpr {

class InferenceScheduler {
 public:
  explicit InferenceScheduler(Settings settings);
  WorkSelection select(std::vector<TrackedVehicle*> tracks, size_t frame_index) const;
  void update(Settings settings);

 private:
  Settings settings_;
};

}  // namespace alpr

