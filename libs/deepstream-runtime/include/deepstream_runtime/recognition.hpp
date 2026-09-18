#pragma once

#include "alpr/event.hpp"
#include "alpr/ocr.hpp"
#include "deepstream_runtime/config.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace deepstream_runtime {

class RecognitionTracker {
 public:
  explicit RecognitionTracker(const PipelineConfig& config);
  RecognitionTracker(const RecognitionTracker&) = delete;
  RecognitionTracker& operator=(const RecognitionTracker&) = delete;

  void mark_seen(uint64_t track_id, uint64_t frame_number);
  std::optional<alpr::AlprEvent> observe(
      const alpr::PlateJob& job, const alpr::PlateDetection& plate,
      const alpr::OcrResult& ocr, const std::string& observed_at);
  std::vector<uint64_t> expire(uint64_t frame_number);
  std::vector<uint64_t> clear();
  size_t size() const;

 private:
  struct TrackState {
    TrackState(size_t maximum_values, float lock_confidence)
        : vote(9U, maximum_values, lock_confidence) {}
    alpr::PlateVote vote;
    uint64_t last_seen_frame{0};
    bool published{false};
    float best_quality{-1.0F};
    alpr::PlateJob best_job;
    alpr::PlateDetection best_plate;
    std::string best_observed_at;
  };

  PipelineConfig config_;
  mutable std::mutex mutex_;
  std::unordered_map<uint64_t, TrackState> tracks_;
};

}  // namespace deepstream_runtime
