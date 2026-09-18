#pragma once

#include "alpr/event.hpp"
#include "alpr/scheduler.hpp"
#include "traffic_alpr/config.hpp"
#include "traffic_alpr/engine_contract.hpp"
#include "traffic_alpr/job_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace traffic_alpr {

struct FrameSubmission {
  bool processed{false};
  bool configured_skip{false};
  bool queue_closed{false};
  size_t jobs_created{0};
};

class ProcessingCoordinator {
 public:
  explicit ProcessingCoordinator(
      const PipelineConfig& config,
      EngineBatchProfiles engine_profiles = EngineBatchProfiles{});
  ProcessingCoordinator(const ProcessingCoordinator&) = delete;
  ProcessingCoordinator& operator=(const ProcessingCoordinator&) = delete;

  FrameSubmission submit_frame(uint64_t frame_number, uint64_t frame_pts_ns,
                               const std::vector<alpr::TrackedVehicle>& tracks,
                               FrameLease frame = {});
  std::vector<PlateWorkItem> take_plate_batch();
  void complete_plate_batch(const std::vector<PlateWorkItem>& jobs,
                            size_t inference_failures = 0U);
  void record_lpr_batch(size_t batch_size);
  void record_frame_drop(size_t count = 1U);
  void record_publish_result(bool success);
  bool claim_final_event(uint64_t track_id);
  void track_exited(uint64_t track_id);
  alpr::HealthEvent health(const std::string& observed_at,
                           const std::string& state, double fps) const;
  void close();

 private:
  PipelineConfig config_;
  alpr::EveryVehiclePlanner planner_;
  alpr::FrameRateGate frame_rate_gate_;
  PlateJobQueue plate_queue_;
  mutable std::mutex metrics_mutex_;
  uint64_t frames_input_{0};
  uint64_t frames_processed_{0};
  uint64_t configured_frame_skips_{0};
  uint64_t frames_dropped_{0};
  uint64_t plate_jobs_created_{0};
  uint64_t plate_jobs_completed_{0};
  uint64_t plate_inference_failures_{0};
  uint64_t vehicles_exited_without_inference_{0};
  uint64_t events_published_{0};
  uint64_t publish_failures_{0};
  std::map<size_t, uint64_t> plate_batch_histogram_;
  std::map<size_t, uint64_t> lpr_batch_histogram_;
  std::unordered_set<uint64_t> tracks_admitted_;
  std::unordered_set<uint64_t> tracks_inferred_;
  std::unordered_set<uint64_t> finalized_tracks_;
};

}  // namespace traffic_alpr
