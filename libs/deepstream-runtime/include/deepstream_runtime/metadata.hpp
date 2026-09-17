#pragma once

#include "alpr/event.hpp"

#include <array>
#include <memory>
#include <string>
#include <utility>

namespace deepstream_runtime {

struct PlateJobMetadata {
  std::string source_id;
  uint64_t frame_number{0};
  uint64_t frame_pts_ns{0};
  uint64_t track_id{0};
  size_t batch_index{0};
  alpr::Detection vehicle;
};

struct PlateKeypointsMetadata {
  std::array<alpr::Point, 5> points{};
};

struct OcrMetadata {
  alpr::OcrResult result;
};

struct FinalEventMetadata {
  alpr::AlprEvent event;
  std::string json;
};

template <typename T>
class OwnedMetadata {
 public:
  explicit OwnedMetadata(T value) : value_(std::make_unique<T>(std::move(value))) {}
  OwnedMetadata(const OwnedMetadata& other)
      : value_(other.value_ ? std::make_unique<T>(*other.value_) : nullptr) {}
  OwnedMetadata& operator=(const OwnedMetadata& other) {
    if (this != &other) {
      value_ = other.value_ ? std::make_unique<T>(*other.value_) : nullptr;
    }
    return *this;
  }
  OwnedMetadata(OwnedMetadata&&) noexcept = default;
  OwnedMetadata& operator=(OwnedMetadata&&) noexcept = default;
  const T& get() const { return *value_; }
  T& get() { return *value_; }

 private:
  std::unique_ptr<T> value_;
};

FinalEventMetadata make_final_event_metadata(alpr::AlprEvent event);

}  // namespace deepstream_runtime
