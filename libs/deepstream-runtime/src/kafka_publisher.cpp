#include "deepstream_runtime/kafka_publisher.hpp"

#include "deepstream_runtime/runtime_control.hpp"

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gstnvdsmeta.h>
#include <nvdsmeta.h>
#include <nvdsmeta_schema.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace deepstream_runtime {
namespace {

gpointer copy_payload(gpointer data, gpointer) {
  const auto* source = static_cast<const NvDsPayload*>(data);
  if (source == nullptr) return nullptr;
  auto* destination = static_cast<NvDsPayload*>(g_malloc0(sizeof(NvDsPayload)));
  destination->payloadSize = source->payloadSize;
  destination->componentId = source->componentId;
  destination->payload = g_memdup2(source->payload, source->payloadSize);
  return destination;
}

void release_payload(gpointer data, gpointer) {
  auto* payload = static_cast<NvDsPayload*>(data);
  if (payload == nullptr) return;
  g_free(payload->payload);
  g_free(payload);
}

GstBuffer* payload_buffer(const std::string& json) {
  GstBuffer* buffer = gst_buffer_new_allocate(nullptr, 1U, nullptr);
  if (buffer == nullptr) return nullptr;
  NvDsBatchMeta* batch = nvds_create_batch_meta(1U);
  if (batch == nullptr) {
    gst_buffer_unref(buffer);
    return nullptr;
  }
  NvDsMeta* gst_meta = gst_buffer_add_nvds_meta(
      buffer, batch, nullptr, nvds_batch_meta_copy_func,
      nvds_batch_meta_release_func);
  if (gst_meta == nullptr) {
    nvds_destroy_batch_meta(batch);
    gst_buffer_unref(buffer);
    return nullptr;
  }
  gst_meta->meta_type = NVDS_BATCH_GST_META;
  batch->base_meta.batch_meta = batch;
  batch->base_meta.copy_func = nvds_batch_meta_copy_func;
  batch->base_meta.release_func = nvds_batch_meta_release_func;
  batch->max_frames_in_batch = 1U;

  NvDsFrameMeta* frame = nvds_acquire_frame_meta_from_pool(batch);
  NvDsUserMeta* user = nvds_acquire_user_meta_from_pool(batch);
  if (frame == nullptr || user == nullptr) {
    gst_buffer_unref(buffer);
    return nullptr;
  }
  auto* payload = static_cast<NvDsPayload*>(g_malloc0(sizeof(NvDsPayload)));
  payload->payloadSize = static_cast<guint>(json.size());
  payload->payload = g_memdup2(json.data(), json.size());
  user->user_meta_data = payload;
  user->base_meta.meta_type = NVDS_PAYLOAD_META;
  user->base_meta.copy_func = copy_payload;
  user->base_meta.release_func = release_payload;
  nvds_add_user_meta_to_frame(frame, user);
  nvds_add_frame_meta_to_batch(batch, frame);
  return buffer;
}

class BrokerBranch {
 public:
  ~BrokerBranch() { stop(); }

  bool start(const std::string& brokers, const std::string& topic,
             size_t queue_capacity) {
    stop();
    pipeline_ = gst_pipeline_new(nullptr);
    appsrc_ = gst_element_factory_make("appsrc", nullptr);
    broker_ = gst_element_factory_make("nvmsgbroker", nullptr);
    if (pipeline_ == nullptr || appsrc_ == nullptr || broker_ == nullptr) {
      stop();
      return false;
    }
    constexpr char kKafkaAdapter[] =
        "/opt/nvidia/deepstream/deepstream/lib/libnvds_kafka_proto.so";
    g_object_set(appsrc_, "is-live", TRUE, "format", GST_FORMAT_TIME, "block", FALSE,
                 "max-buffers", static_cast<guint64>(queue_capacity), nullptr);
    g_object_set(broker_, "proto-lib", kKafkaAdapter, "conn-str", brokers.c_str(),
                 "topic", topic.c_str(), "new-api", TRUE, "sync", FALSE,
                 "async", TRUE, nullptr);
    gst_bin_add_many(GST_BIN(pipeline_), appsrc_, broker_, nullptr);
    if (!gst_element_link(appsrc_, broker_) ||
        gst_element_set_state(pipeline_, GST_STATE_PLAYING) ==
            GST_STATE_CHANGE_FAILURE) {
      stop();
      return false;
    }
    bus_ = gst_element_get_bus(pipeline_);
    return true;
  }

  bool push(const std::string& payload) {
    if (pipeline_ == nullptr || appsrc_ == nullptr || failed()) return false;
    GstBuffer* buffer = payload_buffer(payload);
    if (buffer == nullptr) return false;
    const GstFlowReturn result = gst_app_src_push_buffer(GST_APP_SRC(appsrc_), buffer);
    return result == GST_FLOW_OK && !failed();
  }

  bool failed() {
    if (bus_ == nullptr) return true;
    GstMessage* message = gst_bus_pop_filtered(
        bus_, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    if (message == nullptr) return false;
    gst_message_unref(message);
    return true;
  }

  void stop() {
    if (appsrc_ != nullptr) gst_app_src_end_of_stream(GST_APP_SRC(appsrc_));
    if (pipeline_ != nullptr) gst_element_set_state(pipeline_, GST_STATE_NULL);
    if (bus_ != nullptr) gst_object_unref(bus_);
    if (pipeline_ != nullptr) gst_object_unref(pipeline_);
    bus_ = nullptr;
    pipeline_ = nullptr;
    appsrc_ = nullptr;
    broker_ = nullptr;
  }

 private:
  GstElement* pipeline_{nullptr};
  GstElement* appsrc_{nullptr};
  GstElement* broker_{nullptr};
  GstBus* bus_{nullptr};
};

}  // namespace

class KafkaPublisher::Implementation {
 public:
  Implementation(KafkaConfig config, std::string brokers, ResultHandler handler)
      : config_(std::move(config)), brokers_(std::move(brokers)),
        handler_(std::move(handler)), queue_(config_.queue_capacity),
        backoff_(1000U, 30000U) {}

  ~Implementation() {
    if (thread_.joinable()) thread_.join();
  }

  bool start(std::string* error) {
    if (thread_.joinable()) return true;
    if (brokers_.empty()) {
      *error = "Kafka is enabled but its broker environment variable is unset";
      return false;
    }
    running_.store(true);
    // Reject publications until both broker branches have reached READY. This
    // keeps an outage from turning the bounded process queue into an implicit
    // durable spool and makes every discarded event observable to health
    // accounting through the result handler.
    reconnecting_.store(true);
    thread_ = std::thread(&Implementation::run, this);
    return true;
  }

  bool publish(std::string payload, bool event) {
    if (reconnecting_.load() || degraded_.load()) {
      ++failed_;
      if (handler_) handler_(event, false);
      return false;
    }
    const std::string& topic = event ? config_.event_topic : config_.health_topic;
    const bool accepted = queue_.push({topic, std::move(payload)});
    if (!accepted) {
      degraded_.store(true);
      ++failed_;
      if (handler_) handler_(event, false);
    }
    return accepted;
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
  bool connect() {
    const bool event_ready = events_.start(brokers_, config_.event_topic,
                                           config_.queue_capacity);
    const bool health_ready = event_ready && health_.start(
        brokers_, config_.health_topic, config_.queue_capacity);
    if (!health_ready) {
      events_.stop();
      health_.stop();
      return false;
    }
    backoff_.reset();
    degraded_.store(false);
    return true;
  }

  void run() {
    while (running_.load() || queue_.depth() != 0U) {
      if (!connected_ && !connect()) {
        degraded_.store(true);
        reconnecting_.store(true);
        ++failed_;
        std::this_thread::sleep_for(
            std::chrono::milliseconds(backoff_.next_delay_ms()));
        continue;
      }
      connected_ = true;
      reconnecting_.store(false);
      auto message = queue_.pop();
      if (!message.has_value()) break;
      const bool event = message->topic == config_.event_topic;
      BrokerBranch& branch = event ? events_ : health_;
      const bool success = branch.push(message->payload);
      if (!success) {
        degraded_.store(true);
        connected_ = false;
        reconnecting_.store(true);
        ++failed_;
        events_.stop();
        health_.stop();
      }
      if (handler_) handler_(event, success);
    }
    events_.stop();
    health_.stop();
    finished_.store(true);
  }

  KafkaConfig config_;
  std::string brokers_;
  ResultHandler handler_;
  PublishQueue queue_;
  ReconnectBackoff backoff_;
  BrokerBranch events_;
  BrokerBranch health_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> finished_{false};
  std::atomic<bool> degraded_{false};
  std::atomic<bool> reconnecting_{false};
  std::atomic<uint64_t> failed_{0U};
  bool connected_{false};
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

bool KafkaPublisher::publish_event(std::string payload) {
  return implementation_->publish(std::move(payload), true);
}

bool KafkaPublisher::publish_health(std::string payload) {
  return implementation_->publish(std::move(payload), false);
}

void KafkaPublisher::stop(uint32_t drain_timeout_seconds) {
  if (implementation_ != nullptr &&
      !implementation_->stop(drain_timeout_seconds)) {
    // Some protocol adapters can block inside their disconnect function. The
    // detached worker retains its complete state until process exit so the
    // configured service shutdown deadline is still honored without a use-after-free.
    static_cast<void>(implementation_.release());
  }
}

bool KafkaPublisher::degraded() const {
  return implementation_ == nullptr || implementation_->degraded();
}

bool KafkaPublisher::reconnecting() const {
  return implementation_ == nullptr || implementation_->reconnecting();
}

uint64_t KafkaPublisher::failed_submissions() const {
  return implementation_ == nullptr ? 0U : implementation_->failed();
}

}  // namespace deepstream_runtime
