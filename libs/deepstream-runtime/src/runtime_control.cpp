#include "deepstream_runtime/runtime_control.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace deepstream_runtime {

const char* service_state_name(ServiceState state) {
  switch (state) {
    case ServiceState::kStarting: return "starting";
    case ServiceState::kRunning: return "running";
    case ServiceState::kDegraded: return "degraded";
    case ServiceState::kReconnecting: return "reconnecting";
    case ServiceState::kStopping: return "stopping";
  }
  return "degraded";
}

ReconnectBackoff::ReconnectBackoff(uint32_t initial_ms, uint32_t maximum_ms)
    : initial_ms_(initial_ms), maximum_ms_(maximum_ms), next_ms_(initial_ms) {
  if (initial_ms == 0U || maximum_ms < initial_ms) {
    throw std::invalid_argument("reconnect backoff bounds are invalid");
  }
}

uint32_t ReconnectBackoff::next_delay_ms() {
  const uint32_t current = next_ms_;
  const uint64_t doubled = static_cast<uint64_t>(next_ms_) * 2U;
  next_ms_ = static_cast<uint32_t>(
      std::min<uint64_t>(doubled, static_cast<uint64_t>(maximum_ms_)));
  return current;
}

void ReconnectBackoff::reset() { next_ms_ = initial_ms_; }

PublishQueue::PublishQueue(size_t capacity) : capacity_(capacity) {
  if (capacity == 0U) throw std::invalid_argument("publish queue capacity must be positive");
}

bool PublishQueue::push(PublishMessage message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (closed_ || messages_.size() >= capacity_) {
    ++rejected_;
    return false;
  }
  messages_.push_back(std::move(message));
  ready_.notify_one();
  return true;
}

std::optional<PublishMessage> PublishQueue::pop() {
  std::unique_lock<std::mutex> lock(mutex_);
  ready_.wait(lock, [&] { return closed_ || !messages_.empty(); });
  if (messages_.empty()) return std::nullopt;
  PublishMessage message = std::move(messages_.front());
  messages_.pop_front();
  return message;
}

void PublishQueue::close() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
  }
  ready_.notify_all();
}

size_t PublishQueue::depth() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return messages_.size();
}

uint64_t PublishQueue::rejected() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return rejected_;
}

}  // namespace deepstream_runtime
