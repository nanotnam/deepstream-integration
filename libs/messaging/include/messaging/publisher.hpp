#pragma once

#include "messaging/runtime.hpp"

#include <cstdint>
#include <string>

namespace messaging {

enum class PublishStatus { kAccepted, kUnavailable, kQueueFull, kClosed };

struct PublisherHealth {
  bool degraded{false};
  bool reconnecting{false};
  uint64_t failed_submissions{0};
};

class Publisher {
 public:
  virtual ~Publisher() = default;
  virtual bool start(std::string* error) = 0;
  virtual PublishStatus publish(Message message) = 0;
  virtual void stop(uint32_t drain_timeout_seconds) = 0;
  virtual PublisherHealth health() const = 0;
};

}  // namespace messaging
