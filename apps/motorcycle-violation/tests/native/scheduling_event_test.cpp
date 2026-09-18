#include "alpr/event.hpp"
#include "alpr/ocr.hpp"
#include "alpr/scheduler.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

alpr::TrackedVehicle vehicle(uint64_t track_id, alpr::Box box, bool active = true) {
  alpr::TrackedVehicle result;
  result.track_id = track_id;
  result.active = active;
  result.detection = {3, "car", 0.9F, box};
  return result;
}

}  // namespace

int main() {
  const std::vector<alpr::Point> zone{{0.1F, 0.1F}, {0.9F, 0.1F},
                                      {0.9F, 0.9F}, {0.1F, 0.9F}};
  alpr::EveryVehiclePlanner planner(zone);

  assert(planner.plan("camera", 1U, 100U, {}).empty());
  const auto inside = vehicle(1U, {0.2F, 0.2F, 0.2F, 0.2F});
  const auto boundary = vehicle(2U, {0.0F, 0.5F, 0.2F, 0.4F});
  const auto outside = vehicle(3U, {0.8F, 0.8F, 0.2F, 0.2F});
  const auto inactive = vehicle(4U, {0.2F, 0.2F, 0.2F, 0.2F}, false);
  const auto selected = planner.plan(
      "camera", 7U, 1234U, {inside, boundary, outside, inactive});
  assert(selected.size() == 2U);
  assert(selected[0].track_id == 1U);
  assert(selected[1].track_id == 2U);
  assert(selected[0].frame_number == 7U);
  assert(selected[0].frame_pts_ns == 1234U);

  assert(planner.plan("camera", 10U, 4000U, {inside}).size() == 1U);
  assert(planner.plan("camera", 11U, 5000U, {inside, inside, inside}).size() == 3U);

  // Planning the same active track on the next frame is the retry policy.
  assert(planner.plan("camera", 8U, 2234U, {inside}).size() == 1U);

  std::vector<alpr::TrackedVehicle> eleven;
  for (uint64_t track = 0U; track < 11U; ++track) {
    eleven.push_back(vehicle(track, {0.2F, 0.2F, 0.2F, 0.2F}));
  }
  auto jobs = planner.plan("camera", 9U, 3234U, eleven);
  const auto batches = alpr::split_plate_batches(std::move(jobs), 8U);
  assert(batches.size() == 2U);
  assert(batches[0].jobs.size() == 8U);
  assert(batches[1].jobs.size() == 3U);
  assert(batches[1].jobs[2].track_id == 10U);
  bool rejected_zero_batch = false;
  try {
    static_cast<void>(alpr::split_plate_batches({}, 0U));
  } catch (const std::invalid_argument&) {
    rejected_zero_batch = true;
  }
  assert(rejected_zero_batch);

  alpr::FrameRateGate full_rate(0.0);
  assert(full_rate.accept(0U));
  assert(full_rate.accept(1U));
  alpr::FrameRateGate fifteen_fps(15.0);
  assert(fifteen_fps.accept(0U));
  assert(!fifteen_fps.accept(33333333U));
  assert(fifteen_fps.accept(66666667U));

  alpr::AlprEvent event;
  event.source_id = "camera-01";
  event.track_id = 42U;
  event.observed_at = "2026-09-17T00:00:00Z";
  event.frame_pts_ns = 1000U;
  event.vehicle = {3, "car", 0.9F, {0.1F, 0.2F, 0.3F, 0.4F}};
  event.plate.detection = {0, "plate", 0.8F, {0.2F, 0.3F, 0.1F, 0.05F}};
  event.plate_text = "89AA15689";
  event.plate_confidence = 0.95F;
  event.model_bundle = "1.0.0";
  event.precision = "fp16";
  const std::string id = alpr::deterministic_event_id(event);
  assert(id == alpr::deterministic_event_id(event));
  const std::string json = alpr::event_json(event);
  assert(json.find("mbfs.alpr.event.v1") != std::string::npos);
  assert(json.find(id) != std::string::npos);
  assert(json.find("89AA15689") != std::string::npos);

  alpr::HealthEvent health;
  health.source_id = "camera-01";
  health.observed_at = "2026-09-17T00:00:00Z";
  health.state = "running";
  health.model_bundle = "1.0.0";
  health.precision = "fp16";
  health.plate_jobs_created = 11U;
  health.plate_jobs_completed = 11U;
  health.plate_batch_histogram = {{3U, 1U}, {8U, 1U}};
  const std::string health_payload = alpr::health_json(health);
  assert(health_payload.find("\"jobs_created\":11") != std::string::npos);
  assert(health_payload.find("\"8\":1") != std::string::npos);

  alpr::PlateRegistry registry;
  assert(registry.claim(1U, "89AA15689", 100U, 1000U));
  assert(!registry.claim(2U, "89AA15689", 200U, 1000U));
  assert(registry.claim(2U, "89AA15689", 1100U, 1000U));
  return 0;
}
