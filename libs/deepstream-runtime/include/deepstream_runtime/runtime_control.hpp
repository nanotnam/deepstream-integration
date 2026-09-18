#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace deepstream_runtime {

enum class ServiceState { kStarting, kRunning, kDegraded, kReconnecting, kStopping };

const char* service_state_name(ServiceState state);

class ReconnectBackoff {
 public:
  ReconnectBackoff(uint32_t initial_ms, uint32_t maximum_ms);
  uint32_t next_delay_ms();
  void reset();

 private:
  uint32_t initial_ms_;
  uint32_t maximum_ms_;
  uint32_t next_ms_;
};

struct PublishMessage {
  std::string topic;
  std::string payload;
};

class PublishQueue {
 public:
  explicit PublishQueue(size_t capacity);
  bool push(PublishMessage message);
  std::optional<PublishMessage> pop();
  void close();
  size_t depth() const;
  uint64_t rejected() const;

 private:
  size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<PublishMessage> messages_;
  uint64_t rejected_{0};
  bool closed_{false};
};

}  // namespace deepstream_runtime
