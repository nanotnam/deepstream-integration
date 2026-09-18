#include "motorcycle_violation/kafka_publisher.hpp"

#include "motorcycle_violation/runtime_control.hpp"

#include <dlfcn.h>
#include <nvds_msgapi.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace motorcycle_violation {
namespace {

constexpr char kKafkaAdapter[] =
    "/opt/nvidia/deepstream/deepstream/lib/libnvds_kafka_proto.so";

template <typename Function>
bool load_symbol(void* library, const char* name, Function* function) {
  static_assert(sizeof(Function) == sizeof(void*));
  void* symbol = dlsym(library, name);
  if (symbol == nullptr) return false;
  std::memcpy(function, &symbol, sizeof(symbol));
  return true;
}

}  // namespace

class KafkaPublisher::Implementation {
 public:
  Implementation(KafkaConfig config, std::string brokers, ResultHandler handler)
      : config_(std::move(config)), brokers_(std::move(brokers)),
        handler_(std::move(handler)), queue_(config_.queue_capacity),
        backoff_(1000U, 30000U) {}

  ~Implementation() {
    if (thread_.joinable()) thread_.join();
    if (adapter_ != nullptr) dlclose(adapter_);
  }

  bool start(std::string* error) {
    if (thread_.joinable()) return true;
    if (brokers_.empty()) {
      *error = "Kafka is enabled but its broker environment variable is unset";
      return false;
    }
    if (!load_adapter(error)) return false;
    running_.store(true);
    reconnecting_.store(true);
    thread_ = std::thread(&Implementation::run, this);
    return true;
  }

  messaging::PublishStatus publish(messaging::Message message) {
    const bool event = message.topic == config_.event_topic;
    if (reconnecting_.load() || degraded_.load()) {
      ++failed_;
      if (handler_) handler_(event, false);
      return messaging::PublishStatus::kUnavailable;
    }
    if (!queue_.push(std::move(message))) {
      degraded_.store(true);
      ++failed_;
      if (handler_) handler_(event, false);
      return messaging::PublishStatus::kQueueFull;
    }
    return messaging::PublishStatus::kAccepted;
  }

  bool stop(uint32_t timeout_seconds) {
    if (!running_.exchange(false) && !thread_.joinable()) return true;
    queue_.close();
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(timeout_seconds);
    while (thread_.joinable() && !finished_.load() && timeout_seconds > 0U &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!thread_.joinable()) return true;
    if (finished_.load()) {
      thread_.join();
      return true;
    }
    thread_.detach();
    return false;
  }

  bool degraded() const { return degraded_.load(); }
  bool reconnecting() const { return reconnecting_.load(); }
  uint64_t failed() const { return failed_.load(); }

 private:
  using Connect = decltype(&nvds_msgapi_connect);
  using SendAsync = decltype(&nvds_msgapi_send_async);
  using DoWork = decltype(&nvds_msgapi_do_work);
  using Disconnect = decltype(&nvds_msgapi_disconnect);

  struct PendingSend {
    Implementation* owner;
    bool event;
    std::string payload;
  };

  bool load_adapter(std::string* error) {
    adapter_ = dlopen(kKafkaAdapter, RTLD_NOW | RTLD_LOCAL);
    if (adapter_ == nullptr) {
      *error = std::string("cannot load Kafka protocol adapter: ") + dlerror();
      return false;
    }
    if (!load_symbol(adapter_, "nvds_msgapi_connect", &connect_) ||
        !load_symbol(adapter_, "nvds_msgapi_send_async", &send_async_) ||
        !load_symbol(adapter_, "nvds_msgapi_do_work", &do_work_) ||
        !load_symbol(adapter_, "nvds_msgapi_disconnect", &disconnect_)) {
      *error = std::string("Kafka protocol adapter API is incomplete: ") + dlerror();
      return false;
    }
    return true;
  }

  static void connection_event(NvDsMsgApiHandle, NvDsMsgApiEventType) {}

  static void send_complete(void* context, NvDsMsgApiErrorType result) {
    std::unique_ptr<PendingSend> pending(static_cast<PendingSend*>(context));
    pending->owner->delivery_success_.store(result == NVDS_MSGAPI_OK);
    pending->owner->delivery_finished_.store(true);
    if (pending->owner->handler_) {
      pending->owner->handler_(pending->event, result == NVDS_MSGAPI_OK);
    }
  }

  bool connect() {
    std::string connection = brokers_;
    std::string adapter_config = config_.adapter_config.string();
    handle_ = connect_(connection.data(), connection_event, adapter_config.data());
    if (handle_ == nullptr) return false;
    backoff_.reset();
    degraded_.store(false);
    reconnecting_.store(false);
    return true;
  }

  void disconnect() {
    if (handle_ != nullptr) {
      static_cast<void>(disconnect_(handle_));
      handle_ = nullptr;
    }
  }

  bool send(const messaging::Message& message) {
    auto pending = std::make_unique<PendingSend>(
        PendingSend{this, message.topic == config_.event_topic, message.payload});
    delivery_finished_.store(false);
    delivery_success_.store(false);
    std::string topic = message.topic;
    const NvDsMsgApiErrorType result = send_async_(
        handle_, topic.data(),
        reinterpret_cast<const uint8_t*>(pending->payload.data()),
        pending->payload.size(), send_complete, pending.get());
    if (result != NVDS_MSGAPI_OK) {
      ++failed_;
      if (handler_) handler_(pending->event, false);
      return false;
    }
    static_cast<void>(pending.release());
    while (!delivery_finished_.load()) {
      do_work_(handle_);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!delivery_success_.load()) ++failed_;
    return delivery_success_.load();
  }

  void run() {
    while (running_.load() || queue_.depth() != 0U) {
      if (handle_ == nullptr && !connect()) {
        degraded_.store(true);
        reconnecting_.store(true);
        ++failed_;
        std::this_thread::sleep_for(
            std::chrono::milliseconds(backoff_.next_delay_ms()));
        continue;
      }
      auto message = queue_.pop();
      if (!message.has_value()) break;
      if (!send(*message)) {
        degraded_.store(true);
        reconnecting_.store(true);
        disconnect();
      }
    }
    disconnect();
    finished_.store(true);
  }

  KafkaConfig config_;
  std::string brokers_;
  ResultHandler handler_;
  PublishQueue queue_;
  ReconnectBackoff backoff_;
  void* adapter_{nullptr};
  Connect connect_{nullptr};
  SendAsync send_async_{nullptr};
  DoWork do_work_{nullptr};
  Disconnect disconnect_{nullptr};
  NvDsMsgApiHandle handle_{nullptr};
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> finished_{false};
  std::atomic<bool> degraded_{false};
  std::atomic<bool> reconnecting_{false};
  std::atomic<bool> delivery_finished_{false};
  std::atomic<bool> delivery_success_{false};
  std::atomic<uint64_t> failed_{0U};
};

KafkaPublisher::KafkaPublisher(const KafkaConfig& config, std::string brokers,
                               ResultHandler result_handler)
    : implementation_(std::make_unique<Implementation>(
          config, std::move(brokers), std::move(result_handler))) {}

KafkaPublisher::~KafkaPublisher() {
  if (implementation_ != nullptr && !implementation_->stop(30U)) {
    static_cast<void>(implementation_.release());
  }
}

bool KafkaPublisher::start(std::string* error) {
  return implementation_->start(error);
}

messaging::PublishStatus KafkaPublisher::publish(messaging::Message message) {
  if (implementation_ == nullptr) return messaging::PublishStatus::kClosed;
  return implementation_->publish(std::move(message));
}

void KafkaPublisher::stop(uint32_t drain_timeout_seconds) {
  if (implementation_ != nullptr &&
      !implementation_->stop(drain_timeout_seconds)) {
    static_cast<void>(implementation_.release());
  }
}

messaging::PublisherHealth KafkaPublisher::health() const {
  if (implementation_ == nullptr) return {true, false, 0U};
  return {implementation_->degraded(), implementation_->reconnecting(),
          implementation_->failed()};
}

}  // namespace motorcycle_violation
