#include "traffic_alpr/pipeline_runner.hpp"

#include "traffic_alpr/deepstream_submission.hpp"
#include "traffic_alpr/engine_set.hpp"
#include "traffic_alpr/gpu_inference_executor.hpp"
#include "traffic_alpr/kafka_publisher.hpp"
#include "traffic_alpr/plate_worker.hpp"
#include "traffic_alpr/processing.hpp"
#include "traffic_alpr/runtime_control.hpp"
#include "messaging/publisher.hpp"

#include <NvInferVersion.h>
#include <cuda_runtime_api.h>
#include <glib-unix.h>
#include <gst/gst.h>
#include <nvds_version.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace traffic_alpr {
namespace {

constexpr uint32_t kVehicleComponentId = 1U;

std::string version(unsigned major, unsigned minor, unsigned patch = 0U,
                    bool include_patch = false) {
  std::ostringstream output;
  output << major << '.' << minor;
  if (include_patch) output << '.' << patch;
  return output.str();
}

bool runtime_identity(RuntimeIdentity* identity, std::string* error) {
  int cuda_runtime = 0;
  if (cudaRuntimeGetVersion(&cuda_runtime) != cudaSuccess) {
    *error = "CUDA runtime is unavailable";
    return false;
  }
  cudaDeviceProp properties{};
  if (cudaGetDeviceProperties(&properties, 0) != cudaSuccess) {
    *error = "GPU device 0 is unavailable";
    return false;
  }
  identity->deepstream = version(NVDS_VERSION_MAJOR, NVDS_VERSION_MINOR);
  identity->cuda = version(static_cast<unsigned>(cuda_runtime / 1000),
                           static_cast<unsigned>((cuda_runtime % 1000) / 10));
  identity->tensorrt = version(NV_TENSORRT_MAJOR, NV_TENSORRT_MINOR,
                               NV_TENSORRT_PATCH, true);
  identity->gpu = properties.name;
  identity->gpu_compute_capability = version(
      static_cast<unsigned>(properties.major), static_cast<unsigned>(properties.minor));
  return true;
}

std::string timestamp_now() {
  const auto now = std::chrono::system_clock::now();
  const auto seconds = std::chrono::system_clock::to_time_t(now);
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count() % 1000;
  std::tm value{};
  gmtime_r(&seconds, &value);
  std::ostringstream output;
  output << std::put_time(&value, "%Y-%m-%dT%H:%M:%S") << '.'
         << std::setw(3) << std::setfill('0') << milliseconds << 'Z';
  return output.str();
}

std::filesystem::path plugin_root() {
  const char* configured = std::getenv("ALPR_PLUGIN_ROOT");
  return configured != nullptr && *configured != '\0'
             ? std::filesystem::path(configured)
             : std::filesystem::path("/opt/mbfs/lib");
}

std::filesystem::path bundle_root() {
  const char* configured = std::getenv("ALPR_BUNDLE_ROOT");
  return configured != nullptr && *configured != '\0'
             ? std::filesystem::path(configured)
             : std::filesystem::path("/opt/mbfs/share/traffic-alpr/models/bundles");
}

ModelSourceHashes source_hashes_1_0_0() {
  return {{"vehicle", "acbfdfb436a4224dd855a29476a860f595c331535eca1315263444419f545d77"},
          {"plate", "cb4f612ec169ec8eaf119a60ed7812d392512750737f84feff27565d4e449675"},
          {"lprnet", "86bc30ace8a299c4fe3d5e1ce94a21ef25d0aea2beb7aefaa2041cc4bd917554"}};
}

bool write_vehicle_config(const PipelineConfig& config, const EngineSet& engines,
                          std::filesystem::path* path, std::string* error) {
  const std::filesystem::path temporary =
      std::filesystem::temp_directory_path() /
      ("traffic-alpr-vehicle-" + std::to_string(static_cast<unsigned long>(getpid())) +
       ".txt");
  std::ofstream output(temporary);
  if (!output) {
    *error = "cannot create the temporary vehicle inference configuration";
    return false;
  }
  const auto parser_library = plugin_root() / "libnvdsinfer_custom_alpr.so";
  const auto labels = bundle_root() / config.models.bundle / "vehicle-labels.txt";
  output << "[property]\n"
         << "gpu-id=0\n"
         << "model-engine-file=" << engines.engines.at("vehicle").path.string() << "\n"
         << "labelfile-path=" << labels.string() << "\n"
         << "batch-size=1\nnetwork-mode=2\nnetwork-type=0\n"
         << "num-detected-classes=7\ninterval=0\ngie-unique-id=" << kVehicleComponentId
         << "\nmaintain-aspect-ratio=1\nsymmetric-padding=0\n"
         << "net-scale-factor=0.0090966979\noffsets=125.5;125.5;125.5\n"
         << "model-color-format=0\ncluster-mode=2\n"
         << "custom-lib-path=" << parser_library.string() << "\n"
         << "parse-bbox-func-name=NvDsInferParseMbfsVehicle\n"
         << "output-blob-names=score_8;score_16;score_32;score_64;score_128;"
            "bbox_8;bbox_16;bbox_32;bbox_64;bbox_128;398;401;405;412\n"
         << "[class-attrs-all]\npre-cluster-threshold="
         << config.alpr.vehicle_threshold << "\nnms-iou-threshold="
         << config.alpr.vehicle_iou_threshold << "\n";
  if (!output) {
    *error = "cannot write the temporary vehicle inference configuration";
    return false;
  }
  *path = temporary;
  return true;
}

bool link_decode_pad(GstPad* pad, GstElement* converter) {
  GstCaps* caps = gst_pad_get_current_caps(pad);
  if (caps == nullptr) caps = gst_pad_query_caps(pad, nullptr);
  bool video = false;
  if (caps != nullptr && gst_caps_get_size(caps) > 0U) {
    const GstStructure* structure = gst_caps_get_structure(caps, 0U);
    const char* name = gst_structure_get_name(structure);
    video = name != nullptr && std::string(name).rfind("video/", 0U) == 0U;
    gst_caps_unref(caps);
  }
  if (!video) return true;
  GstPad* sink = gst_element_get_static_pad(converter, "sink");
  if (sink == nullptr) return false;
  const bool result = gst_pad_is_linked(sink) || gst_pad_link(pad, sink) == GST_PAD_LINK_OK;
  gst_object_unref(sink);
  return result;
}

}  // namespace

class PipelineRunner::Implementation {
 public:
  explicit Implementation(PipelineConfig config)
      : config_(std::move(config)), coordinator_(config_),
        source_backoff_(config_.source.reconnect_initial_ms,
                        config_.source.reconnect_maximum_ms) {}

  ~Implementation() { cleanup(); }

  int run(std::string* error) {
    std::string uri;
    if (!resolve_source_uri(config_, [](const char* name) { return std::getenv(name); },
                            &uri, error)) return 65;
    source_uri_ = uri;
    RuntimeIdentity identity;
    if (!runtime_identity(&identity, error)) return 69;
    if (config_.models.bundle != "1.0.0") {
      *error = "the runtime has no source-hash contract for the configured model bundle";
      return 65;
    }
    if (!select_engine_set(config_.models.engine_root, config_.models.bundle,
                           precision_name(config_.models.precision), 1U, identity,
                           source_hashes_1_0_0(), &engines_, error)) return 69;
    EngineBatchProfiles profiles;
    profiles.vehicle = engines_.engines.at("vehicle").batch;
    profiles.plate = engines_.engines.at("plate").batch;
    profiles.lprnet = engines_.engines.at("lprnet").batch;
    if (!validate_engine_batch_profiles(config_, profiles, error)) return 65;
    if (!write_vehicle_config(config_, engines_, &vehicle_config_, error)) return 70;
    executor_ = std::make_unique<GpuInferenceExecutor>(
        config_, engines_, [this](const alpr::AlprEvent& event) { publish_event(event); });
    if (!executor_->initialize(error)) return 69;
    worker_ = std::make_unique<PlateWorker>(
        &coordinator_, [this](const std::vector<PlateWorkItem>& jobs) {
          const size_t failures = executor_->process(jobs);
          if (failures > 0U && !inference_error_reported_.exchange(true)) {
            std::lock_guard<std::mutex> lock(output_mutex_);
            std::cerr << "plate inference degraded: " << executor_->last_error()
                      << std::endl;
          }
          if (failures < jobs.size()) {
            coordinator_.record_lpr_batch(jobs.size() - failures);
          }
          return failures;
        });

    gst_init(nullptr, nullptr);
    if (config_.outputs.kafka.enabled) {
      const char* brokers = std::getenv(config_.outputs.kafka.brokers_env.c_str());
      if (brokers == nullptr || *brokers == '\0') {
        *error = "Kafka is enabled but its broker environment variable is unset";
        return 65;
      }
      publisher_ = std::make_unique<KafkaPublisher>(
          config_.outputs.kafka, brokers,
          [this](bool event, bool success) {
            if (event) coordinator_.record_publish_result(success);
          });
      if (!publisher_->start(error)) return 70;
    }
    loop_ = g_main_loop_new(nullptr, FALSE);
    pipeline_ = gst_pipeline_new("traffic-alpr");
    source_ = gst_element_factory_make("uridecodebin", "source");
    converter_ = gst_element_factory_make("nvvideoconvert", "source-convert");
    caps_filter_ = gst_element_factory_make("capsfilter", "source-caps");
    stream_mux_ = gst_element_factory_make("nvstreammux", "stream-mux");
    vehicle_infer_ = gst_element_factory_make("nvinfer", "vehicle-infer");
    tracker_ = gst_element_factory_make("nvtracker", "tracker");
    sink_ = gst_element_factory_make("fakesink", "sink");
    if (loop_ == nullptr || pipeline_ == nullptr || source_ == nullptr ||
        converter_ == nullptr || caps_filter_ == nullptr || stream_mux_ == nullptr ||
        vehicle_infer_ == nullptr || tracker_ == nullptr || sink_ == nullptr) {
      *error = "a required DeepStream or GStreamer element is unavailable";
      return 69;
    }

    GstCaps* caps = gst_caps_from_string("video/x-raw(memory:NVMM),format=RGBA");
    g_object_set(caps_filter_, "caps", caps, nullptr);
    gst_caps_unref(caps);
    g_object_set(source_, "uri", uri.c_str(), nullptr);
    g_object_set(stream_mux_, "batch-size", 1U, "width", config_.source.width,
                 "height", config_.source.height, "live-source",
                 config_.source.type == SourceType::kRtsp, nullptr);
    g_object_set(vehicle_infer_, "config-file-path", vehicle_config_.c_str(), nullptr);
    const auto tracker_library =
        std::filesystem::path("/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so");
    g_object_set(tracker_, "ll-lib-file", tracker_library.c_str(), "ll-config-file",
                 config_.tracker.config.c_str(), "tracker-width", 960U,
                 "tracker-height", 544U, "gpu-id", 0U, nullptr);
    g_object_set(sink_, "sync", FALSE, "async", FALSE, nullptr);

    gst_bin_add_many(GST_BIN(pipeline_), source_, converter_, caps_filter_, stream_mux_,
                     vehicle_infer_, tracker_, sink_, nullptr);
    if (!gst_element_link(converter_, caps_filter_)) {
      *error = "cannot link source conversion elements";
      return 70;
    }
    GstPad* caps_source = gst_element_get_static_pad(caps_filter_, "src");
    GstPad* mux_sink = gst_element_request_pad_simple(stream_mux_, "sink_0");
    const bool mux_linked = caps_source != nullptr && mux_sink != nullptr &&
                            gst_pad_link(caps_source, mux_sink) == GST_PAD_LINK_OK;
    if (caps_source != nullptr) gst_object_unref(caps_source);
    if (mux_sink != nullptr) gst_object_unref(mux_sink);
    if (!mux_linked || !gst_element_link_many(stream_mux_, vehicle_infer_, tracker_, sink_,
                                               nullptr)) {
      *error = "cannot link the DeepStream inference graph";
      return 70;
    }
    g_signal_connect(source_, "pad-added", G_CALLBACK(on_pad_added), this);
    g_signal_connect(source_, "source-setup", G_CALLBACK(on_source_setup), this);

    GstPad* tracker_source = gst_element_get_static_pad(tracker_, "src");
    if (tracker_source == nullptr) {
      *error = "cannot access the tracker output pad";
      return 70;
    }
    gst_pad_add_probe(tracker_source, GST_PAD_PROBE_TYPE_BUFFER, on_tracker_buffer, this,
                      nullptr);
    gst_object_unref(tracker_source);

    GstBus* bus = gst_element_get_bus(pipeline_);
    bus_watch_ = gst_bus_add_watch(bus, on_bus_message, this);
    gst_object_unref(bus);
    signal_int_ = g_unix_signal_add(SIGINT, on_signal, this);
    signal_term_ = g_unix_signal_add(SIGTERM, on_signal, this);
    publish_health("starting");
    health_timer_ = g_timeout_add_seconds(config_.health.interval_seconds,
                                          on_health_timer, this);
    worker_->start();
    started_at_ = std::chrono::steady_clock::now();
    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
      *error = "the DeepStream pipeline failed to enter PLAYING state";
      return 70;
    }
    g_main_loop_run(loop_);
    cleanup();
    if (!runtime_error_.empty()) {
      *error = runtime_error_;
      return 70;
    }
    return 0;
  }

  void request_stop() {
    if (stop_requested_.exchange(true)) return;
    publish_health("stopping");
    coordinator_.close();
    if (loop_ != nullptr) g_main_loop_quit(loop_);
  }

 private:
  static void on_pad_added(GstElement*, GstPad* pad, gpointer data) {
    auto* self = static_cast<Implementation*>(data);
    if (!link_decode_pad(pad, self->converter_)) {
      self->runtime_error_ = "the decoded video pad could not be linked";
      self->request_stop();
    }
  }

  static void on_source_setup(GstElement*, GstElement* source, gpointer data) {
    auto* self = static_cast<Implementation*>(data);
    GObjectClass* object_class = G_OBJECT_GET_CLASS(source);
    if (g_object_class_find_property(object_class, "latency") != nullptr) {
      g_object_set(source, "latency", self->config_.source.latency_ms, nullptr);
    }
  }

  static GstPadProbeReturn on_tracker_buffer(GstPad*, GstPadProbeInfo* info,
                                             gpointer data) {
    auto* self = static_cast<Implementation*>(data);
    if (self->reconnecting_.exchange(false)) self->source_backoff_.reset();
    GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!submit_deepstream_buffer(
            &self->coordinator_, buffer, kVehicleComponentId,
            [self](uint64_t track_id, uint64_t frame_number) {
              self->executor_->mark_seen(track_id, frame_number);
            },
            [self](uint64_t frame_number) {
              for (const uint64_t track_id : self->executor_->expire(frame_number)) {
                self->coordinator_.track_exited(track_id);
              }
            })) {
      self->runtime_error_ = "tracked frame submission failed";
      self->request_stop();
      return GST_PAD_PROBE_DROP;
    }
    return GST_PAD_PROBE_OK;
  }

  static gboolean on_bus_message(GstBus*, GstMessage* message, gpointer data) {
    auto* self = static_cast<Implementation*>(data);
    switch (GST_MESSAGE_TYPE(message)) {
      case GST_MESSAGE_EOS:
        if (self->config_.source.type == SourceType::kRtsp) {
          self->schedule_reconnect();
        } else {
          self->request_stop();
        }
        break;
      case GST_MESSAGE_ERROR:
        if (self->config_.source.type == SourceType::kRtsp) {
          self->schedule_reconnect();
        } else {
          self->runtime_error_ = "DeepStream pipeline error from element " +
              std::string(GST_OBJECT_NAME(GST_MESSAGE_SRC(message)));
          self->request_stop();
        }
        break;
      default: break;
    }
    return G_SOURCE_CONTINUE;
  }

  static gboolean on_signal(gpointer data) {
    static_cast<Implementation*>(data)->request_stop();
    return G_SOURCE_REMOVE;
  }

  static gboolean on_health_timer(gpointer data) {
    auto* self = static_cast<Implementation*>(data);
    if (self->stop_requested_.load()) return G_SOURCE_REMOVE;
    const messaging::PublisherHealth publisher_health =
        self->publisher_ == nullptr ? messaging::PublisherHealth{}
                                    : self->publisher_->health();
    const bool reconnecting = self->reconnecting_.load() ||
        publisher_health.reconnecting;
    const bool degraded = publisher_health.degraded ||
                          (self->worker_ != nullptr &&
                           self->worker_->failed_batches() > 0U);
    self->publish_health(reconnecting ? "reconnecting" :
                         (degraded ? "degraded" : "running"));
    return G_SOURCE_CONTINUE;
  }

  static gboolean on_reconnect_timer(gpointer data) {
    auto* self = static_cast<Implementation*>(data);
    self->reconnect_timer_ = 0U;
    if (self->stop_requested_.load()) return G_SOURCE_REMOVE;
    if (self->source_ != nullptr) {
      gst_element_set_state(self->source_, GST_STATE_NULL);
      gst_bin_remove(GST_BIN(self->pipeline_), self->source_);
      self->source_ = nullptr;
    }
    self->source_ = gst_element_factory_make("uridecodebin", "source");
    if (self->source_ == nullptr) {
      self->runtime_error_ = "cannot recreate the RTSP source element";
      self->request_stop();
      return G_SOURCE_REMOVE;
    }
    g_object_set(self->source_, "uri", self->source_uri_.c_str(), nullptr);
    g_signal_connect(self->source_, "pad-added", G_CALLBACK(on_pad_added), self);
    g_signal_connect(self->source_, "source-setup", G_CALLBACK(on_source_setup), self);
    const bool added = gst_bin_add(GST_BIN(self->pipeline_), self->source_);
    const bool source_started = added &&
        gst_element_set_state(self->source_, GST_STATE_PLAYING) !=
            GST_STATE_CHANGE_FAILURE;
    const bool pipeline_started = source_started &&
        gst_element_set_state(self->pipeline_, GST_STATE_PLAYING) !=
            GST_STATE_CHANGE_FAILURE;
    if (!pipeline_started) {
      self->schedule_reconnect();
    }
    return G_SOURCE_REMOVE;
  }

  void schedule_reconnect() {
    if (stop_requested_.load() || reconnect_timer_ != 0U) return;
    reconnecting_.store(true);
    // Recreate only source/decode state. Cycling the whole GPU graph for every
    // network error repeatedly tears down NvDCF and nvinfer and is unsafe while
    // bus errors are still in flight.
    if (source_ != nullptr) gst_element_set_state(source_, GST_STATE_NULL);
    if (executor_ != nullptr) {
      for (const uint64_t track_id : executor_->clear()) {
        coordinator_.track_exited(track_id);
      }
    }
    reconnect_timer_ = g_timeout_add(source_backoff_.next_delay_ms(),
                                     on_reconnect_timer, this);
  }

  void write_stdout(const std::string& json) {
    if (!config_.outputs.stdout_enabled) return;
    std::lock_guard<std::mutex> lock(output_mutex_);
    std::cout << json << std::endl;
  }

  void publish_event(const alpr::AlprEvent& event) {
    const std::string json = alpr::event_json(event);
    write_stdout(json);
    if (publisher_ != nullptr) {
      static_cast<void>(publisher_->publish(
          {config_.outputs.kafka.event_topic, json, event.source_id,
           {{"schema", "mbfs.alpr.event.v1"},
            {"event_id", alpr::deterministic_event_id(event)}}}));
    } else {
      coordinator_.record_publish_result(config_.outputs.stdout_enabled);
    }
  }

  void publish_health(const std::string& state) {
    alpr::HealthEvent event = coordinator_.health(timestamp_now(), state, 0.0);
    if (started_at_.time_since_epoch().count() != 0) {
      const double elapsed = std::chrono::duration<double>(
          std::chrono::steady_clock::now() - started_at_).count();
      if (elapsed > 0.0) event.fps = event.frames_processed / elapsed;
    }
    const std::string json = alpr::health_json(event);
    write_stdout(json);
    if (publisher_ != nullptr) {
      static_cast<void>(publisher_->publish(
          {config_.outputs.kafka.health_topic, json, event.source_id,
           {{"schema", "mbfs.alpr.health.v1"}}}));
    }
  }

  void cleanup() {
    if (cleaned_) return;
    cleaned_ = true;
    if (worker_ != nullptr && worker_->running()) worker_->stop();
    if (health_timer_ != 0U) {
      g_source_remove(health_timer_);
      health_timer_ = 0U;
    }
    if (reconnect_timer_ != 0U) {
      g_source_remove(reconnect_timer_);
      reconnect_timer_ = 0U;
    }
    if (executor_ != nullptr) {
      for (const uint64_t track_id : executor_->clear()) {
        coordinator_.track_exited(track_id);
      }
    }
    if (stop_requested_.load()) publish_health("stopping");
    if (publisher_ != nullptr) {
      publisher_->stop(config_.runtime.shutdown_timeout_seconds);
      publisher_.reset();
    }
    if (pipeline_ != nullptr) {
      gst_element_set_state(pipeline_, GST_STATE_NULL);
      gst_object_unref(pipeline_);
      pipeline_ = nullptr;
    }
    if (bus_watch_ != 0U) {
      g_source_remove(bus_watch_);
      bus_watch_ = 0U;
    }
    if (signal_int_ != 0U) {
      g_source_remove(signal_int_);
      signal_int_ = 0U;
    }
    if (signal_term_ != 0U) {
      g_source_remove(signal_term_);
      signal_term_ = 0U;
    }
    if (loop_ != nullptr) {
      g_main_loop_unref(loop_);
      loop_ = nullptr;
    }
    if (!vehicle_config_.empty()) {
      std::error_code ignored;
      std::filesystem::remove(vehicle_config_, ignored);
      vehicle_config_.clear();
    }
  }

  PipelineConfig config_;
  ProcessingCoordinator coordinator_;
  std::unique_ptr<GpuInferenceExecutor> executor_;
  std::unique_ptr<PlateWorker> worker_;
  std::unique_ptr<messaging::Publisher> publisher_;
  ReconnectBackoff source_backoff_;
  EngineSet engines_;
  std::filesystem::path vehicle_config_;
  std::string source_uri_;
  GMainLoop* loop_{nullptr};
  GstElement* pipeline_{nullptr};
  GstElement* source_{nullptr};
  GstElement* converter_{nullptr};
  GstElement* caps_filter_{nullptr};
  GstElement* stream_mux_{nullptr};
  GstElement* vehicle_infer_{nullptr};
  GstElement* tracker_{nullptr};
  GstElement* sink_{nullptr};
  guint bus_watch_{0U};
  guint signal_int_{0U};
  guint signal_term_{0U};
  guint health_timer_{0U};
  guint reconnect_timer_{0U};
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> reconnecting_{false};
  std::atomic<bool> inference_error_reported_{false};
  std::mutex output_mutex_;
  std::chrono::steady_clock::time_point started_at_{};
  std::string runtime_error_;
  bool cleaned_{false};
};

PipelineRunner::PipelineRunner(PipelineConfig config)
    : implementation_(std::make_unique<Implementation>(std::move(config))) {}

PipelineRunner::~PipelineRunner() = default;

int PipelineRunner::run(std::string* error) { return implementation_->run(error); }

void PipelineRunner::request_stop() { implementation_->request_stop(); }

}  // namespace traffic_alpr
