#include "alpr/scheduler.hpp"

#include <algorithm>
#include <utility>

namespace alpr {
namespace {

int priority(const TrackedVehicle& track, size_t frame_index, size_t cache_frames) {
  if (track.newly_created) return 0;
  if (!track.plate) return 1;
  if (frame_index >= track.last_plate_frame + cache_frames) return 2;
  return 3;
}

}  // namespace

InferenceScheduler::InferenceScheduler(Settings settings)
    : settings_(std::move(settings)) {}

WorkSelection InferenceScheduler::select(std::vector<TrackedVehicle*> tracks,
                                         size_t frame_index) const {
  WorkSelection result;
  std::vector<TrackedVehicle*> eligible;
  for (TrackedVehicle* track : tracks) {
    if (track == nullptr || !track->eligible) continue;
    ++result.eligible;
    if (track->finalized) {
      ++result.finalized_suppressed;
      continue;
    }
    eligible.push_back(track);
  }
  std::stable_sort(eligible.begin(), eligible.end(), [&](const TrackedVehicle* lhs,
                                                         const TrackedVehicle* rhs) {
    const int lhs_priority = priority(*lhs, frame_index, settings_.plate_cache_frames);
    const int rhs_priority = priority(*rhs, frame_index, settings_.plate_cache_frames);
    if (lhs_priority != rhs_priority) return lhs_priority < rhs_priority;
    if (lhs->waiting_frames != rhs->waiting_frames) {
      return lhs->waiting_frames > rhs->waiting_frames;
    }
    if (lhs->detection.score != rhs->detection.score) {
      return lhs->detection.score > rhs->detection.score;
    }
    return lhs->track_id < rhs->track_id;
  });

  for (TrackedVehicle* track : eligible) {
    const bool plate_stale = !track->plate ||
        frame_index >= track->last_plate_frame + settings_.plate_cache_frames;
    if (plate_stale &&
        result.plate_track_ids.size() < settings_.max_plate_jobs_per_frame) {
      result.plate_track_ids.push_back(track->track_id);
    } else if (plate_stale) {
      ++result.deferred_plate;
    } else {
      ++result.cached;
      const bool lpr_due = frame_index >= track->last_lpr_frame + settings_.lpr_retry_frames;
      if (lpr_due && result.lpr_track_ids.size() < settings_.max_lpr_jobs_per_frame) {
        result.lpr_track_ids.push_back(track->track_id);
      } else if (lpr_due) {
        ++result.deferred_lpr;
      }
    }
  }
  return result;
}

void InferenceScheduler::update(Settings settings) {
  settings_ = std::move(settings);
}

}  // namespace alpr

