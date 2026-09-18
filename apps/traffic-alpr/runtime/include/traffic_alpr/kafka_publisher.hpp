#pragma once

#include "traffic_alpr/config.hpp"
#include "messaging/publisher.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace traffic_alpr {

class KafkaPublisher final : public messaging::Publisher {
 public:
  using ResultHandler = std::function<void(bool event, bool success)>;

  KafkaPublisher(const KafkaConfig& config, std::string brokers,
                 ResultHandler result_handler);
  ~KafkaPublisher();
  KafkaPublisher(const KafkaPublisher&) = delete;
  KafkaPublisher& operator=(const KafkaPublisher&) = delete;

  bool start(std::string* error) override;
  messaging::PublishStatus publish(messaging::Message message) override;
  void stop(uint32_t drain_timeout_seconds) override;
  messaging::PublisherHealth health() const override;

 private:
  class Implementation;
  std::unique_ptr<Implementation> implementation_;
};

}  // namespace traffic_alpr
