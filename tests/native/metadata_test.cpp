#include "deepstream_runtime/metadata.hpp"

#include <cassert>

int main() {
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
