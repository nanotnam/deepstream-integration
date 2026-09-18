#include "traffic_alpr/metadata.hpp"

#include <utility>

namespace traffic_alpr {

FinalEventMetadata make_final_event_metadata(alpr::AlprEvent event) {
  FinalEventMetadata metadata;
  metadata.json = alpr::event_json(event);
  metadata.event = std::move(event);
  return metadata;
}

}  // namespace traffic_alpr

