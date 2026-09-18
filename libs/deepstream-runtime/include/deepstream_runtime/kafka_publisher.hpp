#pragma once

#include "deepstream_runtime/config.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace deepstream_runtime {

class KafkaPublisher {
 public:
  using ResultHandler = std::function<void(bool event, bool success)>;

  KafkaPublisher(const KafkaConfig& config, std::string brokers,
                 ResultHandler result_handler);
  ~KafkaPublisher();
  KafkaPublisher(const KafkaPublisher&) = delete;
  KafkaPublisher& operator=(const KafkaPublisher&) = delete;

  bool start(std::string* error);
  bool publish_event(std::string payload);
  bool publish_health(std::string payload);
  void stop(uint32_t drain_timeout_seconds);
  bool degraded() const;
  bool reconnecting() const;
  uint64_t failed_submissions() const;

 private:
  class Implementation;
  std::unique_ptr<Implementation> implementation_;
};

}  // namespace deepstream_runtime
