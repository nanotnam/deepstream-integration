#include "deepstream_runtime/config.hpp"
#include "deepstream_runtime/recognition.hpp"

#include <cassert>
#include <filesystem>

namespace {

alpr::OcrResult ocr() {
  return {"89AA15689", std::vector<float>(9U, 0.95F), 0.95F};
}

alpr::PlateJob job(uint64_t frame) {
  return {"replay-01", frame, frame * 1000U, 42U,
          {3, "car", 0.9F, {0.1F, 0.2F, 0.3F, 0.4F}}};
}

alpr::PlateDetection plate() {
  alpr::PlateDetection result;
  result.detection = {0, "plate", 0.8F, {0.2F, 0.3F, 0.1F, 0.05F}};
  return result;
}

}  // namespace

int main() {
  deepstream_runtime::PipelineConfig config;
  std::string error;
  assert(deepstream_runtime::load_pipeline_config(
      std::filesystem::path(TEST_SOURCE_DIR) / "apps/traffic-alpr/configs/file.yaml",
      &config, &error));
  config.tracker.exit_grace_frames = 3U;
  deepstream_runtime::RecognitionTracker tracker(config);
  tracker.mark_seen(42U, 1U);
  assert(!tracker.observe(job(1U), plate(), ocr(), "2026-09-17T00:00:00Z"));
  assert(!tracker.observe(job(2U), plate(), ocr(), "2026-09-17T00:00:01Z"));
  const auto event = tracker.observe(job(3U), plate(), ocr(), "2026-09-17T00:00:02Z");
  assert(event.has_value());
  assert(event->track_id == 42U);
  assert(event->plate_text == "89AA 15689");
  assert(event->model_bundle == "1.0.0");
  assert(!tracker.observe(job(4U), plate(), ocr(), "2026-09-17T00:00:03Z"));
  assert(tracker.size() == 1U);
  assert(tracker.expire(6U).empty());
  const auto expired = tracker.expire(7U);
  assert(expired.size() == 1U && expired.front() == 42U);
  assert(tracker.size() == 0U);
  return 0;
}
