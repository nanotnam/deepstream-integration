#include "motorcycle_violation/processing.hpp"

#include <stdexcept>
#include <utility>

namespace motorcycle_violation {

ProcessingCoordinator::ProcessingCoordinator(
    const PipelineConfig& config, EngineBatchProfiles engine_profiles)
    : config_(config), planner_(config.alpr.recognition_zone),
      frame_rate_gate_(config.processing.max_fps),
      plate_queue_(config.processing.job_queue_capacity) {
  std::string error;
  if (!validate_engine_batch_profiles(config, engine_profiles, &error)) {
    throw std::invalid_argument(error);
  }
}

FrameSubmission ProcessingCoordinator::submit_frame(
    uint64_t frame_number, uint64_t frame_pts_ns,
    const std::vector<alpr::TrackedVehicle>& tracks, FrameLease frame) {
  {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    ++frames_input_;
  }
  if (!frame_rate_gate_.accept(frame_pts_ns)) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    ++configured_frame_skips_;
    return {false, true, false, 0U};
  }

  std::vector<alpr::PlateJob> jobs = planner_.plan(
      config_.source.id, frame_number, frame_pts_ns, tracks);
  FrameSubmission submission{true, false, false, 0U};
  for (alpr::PlateJob& job : jobs) {
    const uint64_t track_id = job.track_id;
    if (!plate_queue_.push({std::move(job), frame})) {
      submission.queue_closed = true;
      break;
    }
    ++submission.jobs_created;
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    ++plate_jobs_created_;
    tracks_admitted_.insert(track_id);
  }
  {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    ++frames_processed_;
  }
  return submission;
}

std::vector<PlateWorkItem> ProcessingCoordinator::take_plate_batch() {
  std::vector<PlateWorkItem> batch =
      plate_queue_.pop_batch(config_.processing.plate_batch_size);
  if (!batch.empty()) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    ++plate_batch_histogram_[batch.size()];
  }
  return batch;
}

void ProcessingCoordinator::complete_plate_batch(
    const std::vector<PlateWorkItem>& jobs, size_t inference_failures) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  plate_jobs_completed_ += jobs.size();
  plate_inference_failures_ += inference_failures;
  for (const PlateWorkItem& item : jobs) {
    tracks_inferred_.insert(item.job.track_id);
  }
}

void ProcessingCoordinator::record_lpr_batch(size_t batch_size) {
  if (batch_size == 0U) return;
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  ++lpr_batch_histogram_[batch_size];
}

void ProcessingCoordinator::record_frame_drop(size_t count) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  frames_dropped_ += count;
}

void ProcessingCoordinator::record_publish_result(bool success) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  if (success) ++events_published_;
  else ++publish_failures_;
}

bool ProcessingCoordinator::claim_final_event(uint64_t track_id) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  return finalized_tracks_.insert(track_id).second;
}

void ProcessingCoordinator::track_exited(uint64_t track_id) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  if (tracks_admitted_.count(track_id) != 0U && tracks_inferred_.count(track_id) == 0U) {
    ++vehicles_exited_without_inference_;
  }
  tracks_admitted_.erase(track_id);
  tracks_inferred_.erase(track_id);
  finalized_tracks_.erase(track_id);
}

alpr::HealthEvent ProcessingCoordinator::health(
    const std::string& observed_at, const std::string& state, double fps) const {
  const JobQueueSnapshot queue = plate_queue_.snapshot();
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  alpr::HealthEvent event;
  event.source_id = config_.source.id;
  event.observed_at = observed_at;
  event.state = state;
  event.fps = fps;
  event.frames_input = frames_input_;
  event.frames_processed = frames_processed_;
  event.frames_dropped = frames_dropped_;
  event.configured_frame_skips = configured_frame_skips_;
  event.plate_jobs_created = plate_jobs_created_;
  event.plate_jobs_completed = plate_jobs_completed_;
  event.plate_inference_failures = plate_inference_failures_;
  event.vehicles_exited_without_inference = vehicles_exited_without_inference_;
  event.queue_depth = queue.depth;
  event.queue_maximum_depth = queue.maximum_depth;
  event.queue_blocked_pushes = queue.blocked_pushes;
  event.queue_blocked_nanoseconds = queue.blocked_nanoseconds;
  event.queue_oldest_job_age_nanoseconds = queue.oldest_job_age_nanoseconds;
  event.plate_batch_histogram = plate_batch_histogram_;
  event.lpr_batch_histogram = lpr_batch_histogram_;
  event.events_published = events_published_;
  event.publish_failures = publish_failures_;
  event.model_bundle = config_.models.bundle;
  event.precision = precision_name(config_.models.precision);
  return event;
}

void ProcessingCoordinator::close() { plate_queue_.close(); }

}  // namespace motorcycle_violation
