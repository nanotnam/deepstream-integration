#include "motorcycle_violation/metadata.hpp"

#include <utility>

namespace motorcycle_violation {

FinalEventMetadata make_final_event_metadata(alpr::AlprEvent event) {
  FinalEventMetadata metadata;
  metadata.json = alpr::event_json(event);
  metadata.event = std::move(event);
  return metadata;
}

}  // namespace motorcycle_violation
