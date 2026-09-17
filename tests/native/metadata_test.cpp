#include "deepstream_runtime/metadata.hpp"

#include <cassert>

int main() {
  deepstream_runtime::PlateJobMetadata job;
  job.source_id = "source";
  job.frame_number = 9U;
  job.track_id = 42U;
  job.batch_index = 3U;
  deepstream_runtime::OwnedMetadata<deepstream_runtime::PlateJobMetadata> owned_job(job);
  auto copied_job = owned_job;
  copied_job.get().track_id = 43U;
  assert(owned_job.get().track_id == 42U);
  assert(copied_job.get().track_id == 43U);

  deepstream_runtime::PlateKeypointsMetadata metadata;
  metadata.points[0] = {0.1F, 0.2F};
  deepstream_runtime::OwnedMetadata<deepstream_runtime::PlateKeypointsMetadata> first(metadata);
  auto second = first;
  second.get().points[0].x = 0.9F;
  assert(first.get().points[0].x == 0.1F);
  assert(second.get().points[0].x == 0.9F);

  alpr::AlprEvent event;
  event.source_id = "source";
  event.plate_text = "89AA15689";
  const auto final = deepstream_runtime::make_final_event_metadata(event);
  assert(final.json.find("mbfs.alpr.event.v1") != std::string::npos);
  return 0;
}
