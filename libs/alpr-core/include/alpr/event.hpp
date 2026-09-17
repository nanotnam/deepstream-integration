#pragma once

#include "alpr/types.hpp"

#include <cstdint>
#include <map>
#include <string>

namespace alpr {

struct AlprEvent {
  std::string source_id;
  uint64_t track_id{0};
  std::string observed_at;
  uint64_t frame_pts_ns{0};
  Detection vehicle;
  PlateDetection plate;
  std::string plate_text;
  float plate_confidence{0.0F};
  std::string model_bundle;
  std::string precision;
};

struct HealthEvent {
  std::string source_id;
  std::string observed_at;
  std::string state;
  double fps{0.0};
  uint64_t frames_input{0};
  uint64_t frames_processed{0};
  uint64_t frames_dropped{0};
  uint64_t configured_frame_skips{0};
  uint64_t plate_jobs_created{0};
  uint64_t plate_jobs_completed{0};
  uint64_t plate_inference_failures{0};
  uint64_t vehicles_exited_without_inference{0};
  uint64_t queue_depth{0};
  uint64_t queue_maximum_depth{0};
  uint64_t queue_blocked_pushes{0};
  uint64_t queue_blocked_nanoseconds{0};
  uint64_t queue_oldest_job_age_nanoseconds{0};
  std::map<size_t, uint64_t> plate_batch_histogram;
  std::map<size_t, uint64_t> lpr_batch_histogram;
  uint64_t events_published{0};
  uint64_t publish_failures{0};
  std::string model_bundle;
  std::string precision;
};

std::string deterministic_event_id(const AlprEvent& event);
std::string event_json(const AlprEvent& event);
std::string health_json(const HealthEvent& event);

}  // namespace alpr
