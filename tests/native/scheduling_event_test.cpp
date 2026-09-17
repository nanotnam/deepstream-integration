#include "alpr/event.hpp"
#include "alpr/ocr.hpp"
#include "alpr/scheduler.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
  alpr::Settings settings;
  settings.max_plate_jobs_per_frame = 1U;
  settings.max_lpr_jobs_per_frame = 1U;
  alpr::InferenceScheduler scheduler(settings);
  alpr::TrackedVehicle first;
  first.track_id = 2U;
  first.eligible = true;
  first.newly_created = true;
  first.detection.score = 0.8F;
  alpr::TrackedVehicle second;
  second.track_id = 1U;
  second.eligible = true;
  second.detection.score = 0.9F;
  alpr::PlateDetection cached;
  cached.detection.score = 0.8F;
  second.plate = cached;
  second.last_plate_frame = 20U;
  const alpr::WorkSelection selection = scheduler.select({&second, &first}, 20U);
  assert(selection.plate_track_ids == std::vector<uint64_t>({2U}));
  assert(selection.lpr_track_ids == std::vector<uint64_t>({1U}));

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

  alpr::PlateRegistry registry;
  assert(registry.claim(1U, "89AA15689", 100U, 1000U));
  assert(!registry.claim(2U, "89AA15689", 200U, 1000U));
  assert(registry.claim(2U, "89AA15689", 1100U, 1000U));
  return 0;
}
