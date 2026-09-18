#include "motorcycle_violation/job_queue.hpp"
#include "motorcycle_violation/processing.hpp"
#include "motorcycle_violation/plate_worker.hpp"

#include <cassert>
#include <chrono>
#include <future>
#include <filesystem>
#include <atomic>
#include <stdexcept>

namespace {

class TestFrameResource final : public motorcycle_violation::FrameResource {
 public:
  explicit TestFrameResource(std::atomic<size_t>* destructions)
      : destructions_(destructions) {}
  ~TestFrameResource() override { destructions_->fetch_add(1U); }

 private:
  std::atomic<size_t>* destructions_;
};

motorcycle_violation::PlateWorkItem job(uint64_t track_id) {
  alpr::PlateJob result;
  result.source_id = "camera";
  result.frame_number = 1U;
  result.track_id = track_id;
  return {std::move(result), {}};
}

}  // namespace

int main() {
  motorcycle_violation::PlateJobQueue queue(2U);
  assert(queue.push(job(1U)));
  assert(queue.push(job(2U)));

  auto producer = std::async(std::launch::async, [&queue] { return queue.push(job(3U)); });
  assert(producer.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout);
  const auto first = queue.pop_batch(1U);
  assert(first.size() == 1U && first[0].job.track_id == 1U);
  assert(producer.get());

  const auto second = queue.pop_batch(8U);
  assert(second.size() == 2U);
  assert(second[0].job.track_id == 2U && second[1].job.track_id == 3U);
  const auto snapshot = queue.snapshot();
  assert(snapshot.depth == 0U);
  assert(snapshot.maximum_depth == 2U);
  assert(snapshot.jobs_pushed == 3U);
  assert(snapshot.jobs_popped == 3U);
  assert(snapshot.blocked_pushes == 1U);
  assert(snapshot.blocked_nanoseconds > 0U);

  queue.close();
  assert(!queue.push(job(4U)));
  assert(queue.pop_batch(1U).empty());

  bool rejected_zero = false;
  try {
    motorcycle_violation::PlateJobQueue invalid(0U);
  } catch (const std::invalid_argument&) {
    rejected_zero = true;
  }
  assert(rejected_zero);

  std::atomic<size_t> frame_destructions{0U};
  motorcycle_violation::PlateJobQueue lease_queue(1U);
  auto leased_job = job(9U);
  leased_job.frame = std::make_shared<TestFrameResource>(&frame_destructions);
  assert(lease_queue.push(std::move(leased_job)));
  assert(frame_destructions.load() == 0U);
  auto leased_batch = lease_queue.pop_batch(1U);
  assert(frame_destructions.load() == 0U);
  leased_batch.clear();
  assert(frame_destructions.load() == 1U);
  lease_queue.close();

  motorcycle_violation::PipelineConfig config;
  std::string error;
  assert(motorcycle_violation::load_pipeline_config(
      std::filesystem::path(TEST_SOURCE_DIR) / "apps/motorcycle-violation/configs/file.yaml",
      &config, &error));
  config.processing.plate_batch_size = 4U;
  motorcycle_violation::EngineBatchProfiles profiles;
  profiles.plate = {1U, 4U, 4U, true};
  motorcycle_violation::ProcessingCoordinator coordinator(config, profiles);
  std::vector<alpr::TrackedVehicle> tracks;
  for (uint64_t track = 0U; track < 11U; ++track) {
    tracks.push_back({track, {3, "car", 0.9F, {0.1F, 0.1F, 0.2F, 0.2F}}, true});
  }
  const auto submission = coordinator.submit_frame(1U, 0U, tracks);
  assert(submission.processed && submission.jobs_created == 11U);
  const auto batch1 = coordinator.take_plate_batch();
  const auto batch2 = coordinator.take_plate_batch();
  const auto batch3 = coordinator.take_plate_batch();
  assert(batch1.size() == 4U && batch2.size() == 4U && batch3.size() == 3U);
  coordinator.complete_plate_batch(batch1);
  coordinator.complete_plate_batch(batch2);
  coordinator.complete_plate_batch(batch3);

  // A completed track is submitted again on its next accepted frame.
  const auto repeated = coordinator.submit_frame(2U, 33333333U, {tracks.front()});
  assert(repeated.jobs_created == 1U);
  const auto repeated_batch = coordinator.take_plate_batch();
  coordinator.complete_plate_batch(repeated_batch);
  assert(coordinator.claim_final_event(0U));
  assert(!coordinator.claim_final_event(0U));
  // Finalization affects event publication only, never future plate admission.
  const auto after_final = coordinator.submit_frame(3U, 66666667U, {tracks.front()});
  assert(after_final.jobs_created == 1U);
  const auto after_final_batch = coordinator.take_plate_batch();
  coordinator.complete_plate_batch(after_final_batch);
  coordinator.record_lpr_batch(1U);
  coordinator.record_frame_drop();
  coordinator.record_publish_result(true);
  coordinator.record_publish_result(false);
  coordinator.track_exited(0U);
  const auto health = coordinator.health("2026-09-17T00:00:00Z", "running", 30.0);
  assert(health.plate_jobs_created == 13U);
  assert(health.plate_jobs_completed == 13U);
  assert(health.vehicles_exited_without_inference == 0U);
  assert(health.frames_dropped == 1U);
  assert(health.events_published == 1U);
  assert(health.publish_failures == 1U);
  assert(health.lpr_batch_histogram.at(1U) == 1U);
  assert(health.plate_batch_histogram.at(4U) == 2U);
  assert(health.plate_batch_histogram.at(3U) == 1U);
  assert(health.plate_batch_histogram.at(1U) == 2U);
  coordinator.close();

  // A worker drains while submission blocks, so a frame may contain more jobs than
  // queue capacity without dropping or deadlocking.
  auto small_queue_config = config;
  small_queue_config.processing.plate_batch_size = 4U;
  small_queue_config.processing.job_queue_capacity = 4U;
  motorcycle_violation::ProcessingCoordinator draining(small_queue_config, profiles);
  std::atomic<size_t> handled{0U};
  motorcycle_violation::PlateWorker worker(
      &draining, [&handled](const std::vector<motorcycle_violation::PlateWorkItem>& jobs) {
        handled.fetch_add(jobs.size());
        return 0U;
      });
  worker.start();
  const auto draining_submission = draining.submit_frame(1U, 0U, tracks);
  assert(draining_submission.jobs_created == 11U);
  worker.stop();
  assert(handled.load() == 11U);
  const auto drained_health = draining.health(
      "2026-09-17T00:00:00Z", "stopping", 30.0);
  assert(drained_health.plate_jobs_created == 11U);
  assert(drained_health.plate_jobs_completed == 11U);

  motorcycle_violation::ProcessingCoordinator failing(config, profiles);
  motorcycle_violation::PlateWorker failing_worker(
      &failing, [](const std::vector<motorcycle_violation::PlateWorkItem>&) -> size_t {
        throw std::runtime_error("synthetic inference failure");
      });
  failing_worker.start();
  assert(failing.submit_frame(1U, 0U, {tracks.front()}).jobs_created == 1U);
  failing_worker.stop();
  const auto failed_health = failing.health(
      "2026-09-17T00:00:00Z", "degraded", 30.0);
  assert(failing_worker.failed_batches() == 1U);
  assert(failed_health.plate_jobs_completed == 1U);
  assert(failed_health.plate_inference_failures == 1U);
  return 0;
}
