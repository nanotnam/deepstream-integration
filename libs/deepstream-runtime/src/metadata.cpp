#include "deepstream_runtime/metadata.hpp"

#include <utility>

namespace deepstream_runtime {

FinalEventMetadata make_final_event_metadata(alpr::AlprEvent event) {
  FinalEventMetadata metadata;
  metadata.json = alpr::event_json(event);
  metadata.event = std::move(event);
  return metadata;
}

}  // namespace deepstream_runtime

