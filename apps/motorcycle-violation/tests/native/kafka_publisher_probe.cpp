#include "motorcycle_violation/config.hpp"
#include "motorcycle_violation/kafka_publisher.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: kafka_publisher_probe <host;port> <adapter-config>\n";
    return 64;
  }
  std::cerr << "probe: construct publisher" << std::endl;
  motorcycle_violation::KafkaConfig config;
  config.adapter_config = argv[2];
  std::atomic<bool> delivered{false};
  motorcycle_violation::KafkaPublisher publisher(
      config, argv[1], [&delivered](bool event, bool success) {
        if (event && success) delivered.store(true);
      });
  std::string error;
  std::cerr << "probe: start publisher" << std::endl;
  if (!publisher.start(&error)) {
    std::cerr << error << '\n';
    return 70;
  }
  std::cerr << "probe: wait for connection" << std::endl;
  const auto ready_deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(30);
  while (publisher.health().reconnecting &&
         std::chrono::steady_clock::now() < ready_deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  if (publisher.health().reconnecting) {
    std::cerr << "Kafka publisher did not become ready\n";
    publisher.stop(2U);
    return 70;
  }
  std::cerr << "probe: publish" << std::endl;
  const std::string payload =
      "{\"schema\":\"mbfs.kafka-key-qualification.v1\","
      "\"source_id\":\"qualification-source\","
      "\"marker\":\"motorcycle-violation-key-probe\"}";
  const auto status = publisher.publish(
      {config.event_topic, payload, "qualification-source",
       {{"schema", "mbfs.kafka-key-qualification.v1"}}});
  if (status != messaging::PublishStatus::kAccepted) {
    std::cerr << "Kafka publisher rejected qualification message\n";
    publisher.stop(2U);
    return 70;
  }
  const auto delivery_deadline = std::chrono::steady_clock::now() +
                                 std::chrono::seconds(15);
  while (!delivered.load() && std::chrono::steady_clock::now() < delivery_deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::cerr << "probe: stop publisher" << std::endl;
  publisher.stop(5U);
  if (!delivered.load()) {
    std::cerr << "Kafka publisher did not confirm delivery\n";
    return 70;
  }
  std::cout << payload << '\n';
  return 0;
}
