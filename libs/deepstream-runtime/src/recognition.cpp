#include "deepstream_runtime/recognition.hpp"

#include <algorithm>
#include <utility>

namespace deepstream_runtime {

RecognitionTracker::RecognitionTracker(const PipelineConfig& config) : config_(config) {}

void RecognitionTracker::mark_seen(uint64_t track_id, uint64_t frame_number) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto [item, inserted] = tracks_.try_emplace(
      track_id, config_.alpr.maximum_values_per_index,
      config_.alpr.character_lock_confidence);
  static_cast<void>(inserted);
  item->second.last_seen_frame = frame_number;
}

std::optional<alpr::AlprEvent> RecognitionTracker::observe(
    const alpr::PlateJob& job, const alpr::PlateDetection& plate,
    const alpr::OcrResult& ocr, const std::string& observed_at) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto [item, inserted] = tracks_.try_emplace(
      job.track_id, config_.alpr.maximum_values_per_index,
      config_.alpr.character_lock_confidence);
  static_cast<void>(inserted);
  TrackState& state = item->second;
  state.last_seen_frame = job.frame_number;
  const float quality = std::clamp(plate.detection.score * ocr.confidence, 0.0F, 1.0F);
  const size_t previous_observations = state.vote.observations();
  static_cast<void>(state.vote.add(ocr, quality));
  if (state.vote.observations() == previous_observations) return std::nullopt;
  if (quality > state.best_quality) {
    state.best_quality = quality;
    state.best_job = job;
    state.best_plate = plate;
    state.best_observed_at = observed_at;
  }
  if (state.published) return std::nullopt;
  const auto text = state.vote.finalize(config_.alpr.minimum_finalize_observations);
  if (!text.has_value()) return std::nullopt;

  state.published = true;
  alpr::AlprEvent event;
  event.source_id = state.best_job.source_id;
  event.track_id = state.best_job.track_id;
  event.observed_at = state.best_observed_at;
  event.frame_pts_ns = state.best_job.frame_pts_ns;
  event.vehicle = state.best_job.vehicle;
  event.plate = state.best_plate;
  event.plate_text = *text;
  event.plate_confidence = state.vote.confidence();
  event.model_bundle = config_.models.bundle;
  event.precision = precision_name(config_.models.precision);
  return event;
}

std::vector<uint64_t> RecognitionTracker::expire(uint64_t frame_number) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<uint64_t> expired;
  for (auto item = tracks_.begin(); item != tracks_.end();) {
    const uint64_t age = frame_number >= item->second.last_seen_frame
                             ? frame_number - item->second.last_seen_frame
                             : 0U;
    if (age >= config_.tracker.exit_grace_frames) {
      expired.push_back(item->first);
      item = tracks_.erase(item);
    } else {
      ++item;
    }
  }
  return expired;
}

std::vector<uint64_t> RecognitionTracker::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<uint64_t> removed;
  removed.reserve(tracks_.size());
  for (const auto& [track_id, state] : tracks_) {
    static_cast<void>(state);
    removed.push_back(track_id);
  }
  tracks_.clear();
  return removed;
}

size_t RecognitionTracker::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return tracks_.size();
}

}  // namespace deepstream_runtime
