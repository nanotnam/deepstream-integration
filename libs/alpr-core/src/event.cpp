#include "alpr/event.hpp"
#include "alpr/ocr.hpp"

#include <array>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace alpr {
namespace {

std::string escape_json(std::string_view value) {
  std::ostringstream stream;
  for (const unsigned char character : value) {
    switch (character) {
      case '"': stream << "\\\""; break;
      case '\\': stream << "\\\\"; break;
      case '\b': stream << "\\b"; break;
      case '\f': stream << "\\f"; break;
      case '\n': stream << "\\n"; break;
      case '\r': stream << "\\r"; break;
      case '\t': stream << "\\t"; break;
      default:
        if (character < 0x20U) {
          stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(character) << std::dec;
        } else {
          stream << static_cast<char>(character);
        }
    }
  }
  return stream.str();
}

uint64_t fnv1a(std::string_view value, uint64_t seed) {
  uint64_t hash = seed;
  for (const unsigned char character : value) {
    hash ^= character;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string box_json(const Box& box) {
  std::ostringstream stream;
  stream << std::setprecision(8) << "{\"x\":" << box.x << ",\"y\":" << box.y
         << ",\"width\":" << box.width << ",\"height\":" << box.height << '}';
  return stream.str();
}

std::string stable_material(const AlprEvent& event) {
  return event.source_id + "\n" + std::to_string(event.track_id) + "\n" +
         std::to_string(event.frame_pts_ns) + "\n" + normalize_plate(event.plate_text);
}

}  // namespace

std::string deterministic_event_id(const AlprEvent& event) {
  const std::string material = stable_material(event);
  const uint64_t first = fnv1a(material, 14695981039346656037ULL);
  const uint64_t second = fnv1a(material, 1099511628211ULL);
  std::ostringstream stream;
  stream << "evt-" << std::hex << std::setfill('0') << std::setw(16) << first
         << std::setw(16) << second;
  return stream.str();
}

std::string event_json(const AlprEvent& event) {
  std::ostringstream stream;
  stream << std::setprecision(8)
         << "{\"schema\":\"mbfs.alpr.event.v1\",\"event_id\":\""
         << deterministic_event_id(event) << "\",\"source_id\":\""
         << escape_json(event.source_id) << "\",\"track_id\":" << event.track_id
         << ",\"observed_at\":\"" << escape_json(event.observed_at)
         << "\",\"frame_pts_ns\":" << event.frame_pts_ns
         << ",\"vehicle\":{\"class_id\":" << event.vehicle.class_id
         << ",\"class\":\"" << escape_json(event.vehicle.label)
         << "\",\"confidence\":" << event.vehicle.score
         << ",\"box\":" << box_json(event.vehicle.box)
         << "},\"plate\":{\"text\":\"" << escape_json(event.plate_text)
         << "\",\"confidence\":" << event.plate_confidence
         << ",\"detection_confidence\":" << event.plate.detection.score
         << ",\"box\":" << box_json(event.plate.detection.box)
         << "},\"model_bundle\":\"" << escape_json(event.model_bundle)
         << "\",\"precision\":\"" << escape_json(event.precision) << "\"}";
  return stream.str();
}

std::string health_json(const HealthEvent& event) {
  std::ostringstream stream;
  stream << std::setprecision(8)
         << "{\"schema\":\"mbfs.alpr.health.v1\",\"source_id\":\""
         << escape_json(event.source_id) << "\",\"observed_at\":\""
         << escape_json(event.observed_at) << "\",\"state\":\""
         << escape_json(event.state) << "\",\"fps\":" << event.fps
         << ",\"frames\":{\"input\":" << event.frames_input
         << ",\"processed\":" << event.frames_processed
         << ",\"dropped\":" << event.frames_dropped
         << "},\"publishing\":{\"events\":" << event.events_published
         << ",\"failures\":" << event.publish_failures
         << "},\"model_bundle\":\"" << escape_json(event.model_bundle)
         << "\",\"precision\":\"" << escape_json(event.precision) << "\"}";
  return stream.str();
}

}  // namespace alpr
