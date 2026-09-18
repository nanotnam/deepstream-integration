#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace messaging {

struct Message {
  Message() = default;
  Message(std::string selected_topic, std::string selected_payload,
          std::string selected_key = {},
          std::map<std::string, std::string> selected_headers = {})
      : topic(std::move(selected_topic)), payload(std::move(selected_payload)),
        key(std::move(selected_key)), headers(std::move(selected_headers)) {}

  std::string topic;
  std::string payload;
  std::string key;
  std::map<std::string, std::string> headers;
};

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

class PublishQueue {
 public:
  explicit PublishQueue(size_t capacity);
  bool push(Message message);
  std::optional<Message> pop();
  void close();
  size_t depth() const;
  uint64_t rejected() const;

 private:
  size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<Message> messages_;
  uint64_t rejected_{0};
  bool closed_{false};
};

}  // namespace messaging
