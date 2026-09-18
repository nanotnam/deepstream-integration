#include "traffic_alpr/gpu_inference_executor.hpp"

#include "alpr/detection.hpp"
#include "alpr/geometry.hpp"
#include "alpr/ocr.hpp"
#include "traffic_alpr/deepstream_frame.hpp"
#include "traffic_alpr/recognition.hpp"
#include "gpu_preprocess.hpp"

#include <cuda_runtime_api.h>
#include <gst/gst.h>
#include <nvbufsurface.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <nvdsinfer_context.h>
#pragma GCC diagnostic pop

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace traffic_alpr {
namespace {

constexpr size_t kPlateElements = 3U * 640U * 640U;
constexpr size_t kLprElements = 32U * 156U * 3U;

void infer_log(NvDsInferContextHandle, unsigned, NvDsInferLogLevel,
               const char*, void*) {}

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

bool copy_path(char* destination, size_t capacity,
               const std::filesystem::path& path) {
  const std::string text = path.string();
  if (text.size() >= capacity) return false;
  std::copy(text.begin(), text.end(), destination);
  destination[text.size()] = '\0';
  return true;
}

class InferContext {
 public:
  ~InferContext() {
    if (handle_ != nullptr) handle_->destroy();
    if (input_ != nullptr) cudaFree(input_);
  }

  bool initialize(const std::filesystem::path& engine, const std::string& input_name,
                  std::vector<int> dimensions, size_t input_elements,
                  unsigned unique_id, std::string* error) {
    NvDsInferContextInitParams parameters;
    NvDsInferContext_ResetInitParams(&parameters);
    if (!copy_path(parameters.modelEngineFilePath,
                   sizeof(parameters.modelEngineFilePath), engine)) {
      *error = "engine path exceeds the DeepStream ABI limit";
      return false;
    }
    parameters.uniqueID = unique_id;
    parameters.networkMode = NvDsInferNetworkMode_FP16;
    parameters.networkType = NvDsInferNetworkType_Other;
    parameters.maxBatchSize = 1U;
    parameters.gpuID = 0U;
    parameters.outputBufferPoolSize = 2U;
    parameters.inputFromPreprocessedTensor = 1;
    parameters.copyInputToHostBuffers = 0;
    const NvDsInferStatus status = createNvDsInferContext(
        &handle_, parameters, nullptr, infer_log);
    if (status != NVDSINFER_SUCCESS || handle_ == nullptr) {
      *error = "DeepStream could not create an inference context";
      return false;
    }
    handle_->fillLayersInfo(layers_);
    const auto input_layer = std::find_if(
        layers_.begin(), layers_.end(), [&](const NvDsInferLayerInfo& layer) {
          return layer.isInput != 0 && layer.layerName != nullptr &&
                 input_name == layer.layerName;
        });
    if (input_layer == layers_.end()) {
      *error = "engine input binding does not match the model contract";
      return false;
    }
    if (cudaMalloc(&input_, input_elements * sizeof(float)) != cudaSuccess) {
      *error = "cannot allocate the GPU inference input";
      return false;
    }
    input_layer_ = *input_layer;
    input_layer_.dataType = FLOAT;
    input_layer_.buffer = input_;
    input_layer_.inferDims.numDims = static_cast<unsigned>(dimensions.size());
    input_layer_.inferDims.numElements = input_elements;
    for (size_t index = 0U; index < dimensions.size(); ++index) {
      input_layer_.inferDims.d[index] = dimensions[index];
    }
    return true;
  }

  float* input() const { return static_cast<float*>(input_); }

  bool infer(std::vector<alpr::TensorView>* outputs, std::string* error) {
    NvDsInferContextBatchPreprocessedInput input{};
    input.tensors = &input_layer_;
    input.numInputTensors = 1U;
    if (handle_->queueInputBatchPreprocessed(input) != NVDSINFER_SUCCESS) {
      *error = "DeepStream rejected a preprocessed inference tensor";
      return false;
    }
    NvDsInferContextBatchOutput batch{};
    if (handle_->dequeueOutputBatch(batch) != NVDSINFER_SUCCESS) {
      *error = "DeepStream failed to return inference output";
      return false;
    }
    outputs->clear();
    size_t output_count = 0U;
    bool valid = true;
    for (const NvDsInferLayerInfo& layer : layers_) {
      if (layer.isInput != 0) continue;
      if (layer.bindingIndex < 0 ||
          static_cast<unsigned>(layer.bindingIndex) >= batch.numHostBuffers ||
          layer.dataType != FLOAT) {
        valid = false;
        break;
      }
      alpr::TensorView tensor;
      tensor.name = layer.layerName == nullptr ? "" : layer.layerName;
      tensor.data_type = alpr::DataType::kFloat32;
      tensor.data = batch.hostBuffers[layer.bindingIndex];
      ++output_count;
      size_t count = 1U;
      for (unsigned index = 0U; index < layer.inferDims.numDims; ++index) {
        const int dimension = layer.inferDims.d[index];
        if (dimension <= 0) {
          valid = false;
          break;
        }
        tensor.shape.push_back(dimension);
        count *= static_cast<size_t>(dimension);
      }
      if (!valid) break;
      if (tensor.shape.empty() || tensor.shape.front() != 1) {
        tensor.shape.insert(tensor.shape.begin(), 1);
      }
      tensor.byte_size = count * sizeof(float);
      outputs->push_back(std::move(tensor));
    }
    const size_t expected_outputs = static_cast<size_t>(std::count_if(
        layers_.begin(), layers_.end(),
        [](const NvDsInferLayerInfo& layer) { return layer.isInput == 0; }));
    if (!valid || output_count != expected_outputs) {
      *error = "engine output bindings do not match the model contract";
    }
    if (valid) {
      owned_batch_ = batch;
      owns_batch_ = true;
    } else {
      handle_->releaseBatchOutput(batch);
    }
    return valid;
  }

  void release() {
    if (owns_batch_) {
      handle_->releaseBatchOutput(owned_batch_);
      owned_batch_ = {};
      owns_batch_ = false;
    }
  }

 private:
  NvDsInferContextHandle handle_{nullptr};
  std::vector<NvDsInferLayerInfo> layers_;
  NvDsInferLayerInfo input_layer_{};
  void* input_{nullptr};
  NvDsInferContextBatchOutput owned_batch_{};
  bool owns_batch_{false};
};

struct MappedSurface {
  GstMapInfo mapping{};
  NvBufSurface* surface{nullptr};
  GstBuffer* buffer{nullptr};

  ~MappedSurface() {
    if (buffer != nullptr) gst_buffer_unmap(buffer, &mapping);
  }
};

bool map_surface(const PlateWorkItem& item, MappedSurface* mapped,
                 std::string* error) {
  const auto frame = std::dynamic_pointer_cast<GstBufferFrameResource>(item.frame);
  if (!frame) {
    *error = "plate work item has no DeepStream frame lease";
    return false;
  }
  mapped->buffer = frame->buffer();
  if (!gst_buffer_map(mapped->buffer, &mapped->mapping, GST_MAP_READ)) {
    mapped->buffer = nullptr;
    *error = "cannot map the DeepStream frame surface";
    return false;
  }
  mapped->surface = reinterpret_cast<NvBufSurface*>(mapped->mapping.data);
  if (mapped->surface == nullptr || mapped->surface->numFilled == 0U ||
      mapped->surface->surfaceList == nullptr) {
    *error = "DeepStream frame surface is empty";
    return false;
  }
  const NvBufSurfaceParams& parameters = mapped->surface->surfaceList[0];
  if (parameters.colorFormat != NVBUF_COLOR_FORMAT_RGBA ||
      parameters.dataPtr == nullptr) {
    *error = "DeepStream worker requires a CUDA RGBA frame surface";
    return false;
  }
  return true;
}

}  // namespace

class GpuInferenceExecutor::Implementation {
 public:
  Implementation(const PipelineConfig& config, const EngineSet& engines,
                 EventHandler handler)
      : config_(config), engines_(engines), handler_(std::move(handler)),
        recognition_(config) {}

  ~Implementation() {
    if (stream_ != nullptr) cudaStreamDestroy(stream_);
  }

  bool initialize(std::string* error) {
    if (cudaSetDevice(0) != cudaSuccess || cudaStreamCreate(&stream_) != cudaSuccess) {
      *error = "cannot initialize the GPU inference stream";
      return false;
    }
    if (!plate_.initialize(engines_.engines.at("plate").path, "img",
                           {1, 3, 640, 640}, kPlateElements, 2U, error) ||
        !lpr_.initialize(engines_.engines.at("lprnet").path, "inputs:0",
                         {1, 32, 156, 3}, kLprElements, 3U, error)) {
      return false;
    }
    return true;
  }

  size_t process(const std::vector<PlateWorkItem>& jobs) {
    size_t failures = 0U;
    for (const PlateWorkItem& item : jobs) {
      std::string error;
      if (!process_one(item, &error)) {
        ++failures;
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = std::move(error);
      }
    }
    return failures;
  }

  void mark_seen(uint64_t track_id, uint64_t frame_number) {
    recognition_.mark_seen(track_id, frame_number);
  }

  std::vector<uint64_t> expire(uint64_t frame_number) {
    return recognition_.expire(frame_number);
  }

  std::vector<uint64_t> clear() { return recognition_.clear(); }

  std::string last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
  }

 private:
  bool process_one(const PlateWorkItem& item, std::string* error) {
    MappedSurface mapped;
    if (!map_surface(item, &mapped, error)) return false;
    const NvBufSurfaceParams& surface = mapped.surface->surfaceList[0];
    const alpr::Box& roi = item.job.vehicle.box;
    const float roi_x = std::clamp(roi.x, 0.0F, 1.0F) * surface.width;
    const float roi_y = std::clamp(roi.y, 0.0F, 1.0F) * surface.height;
    const float roi_width = std::min(roi.width * surface.width,
                                     static_cast<float>(surface.width) - roi_x);
    const float roi_height = std::min(roi.height * surface.height,
                                      static_cast<float>(surface.height) - roi_y);
    if (!launch_plate_preprocess_rgba(
            static_cast<const uint8_t*>(surface.dataPtr), surface.pitch,
            surface.width, surface.height, roi_x, roi_y, roi_width, roi_height,
            plate_.input(), stream_)) {
      *error = "GPU vehicle crop preprocessing failed";
      return false;
    }
    if (cudaStreamSynchronize(stream_) != cudaSuccess) {
      *error = "GPU vehicle crop synchronization failed";
      return false;
    }
    std::vector<alpr::TensorView> plate_outputs;
    if (!plate_.infer(&plate_outputs, error)) return false;
    const float scale = std::min(640.0F / roi_width, 640.0F / roi_height);
    alpr::PlateDetection plate;
    bool found = false;
    const alpr::ImageTransform transform{scale, scale, 0.0F, 0.0F, 640U, 640U};
    const bool decoded = alpr::decode_plate_scrfd(
        static_cast<uint32_t>(std::max(1.0F, roi_width)),
        static_cast<uint32_t>(std::max(1.0F, roi_height)), transform,
        plate_outputs, config_.alpr, &plate, &found, error);
    plate_.release();
    if (!decoded) return false;
    if (!found) return true;
    alpr::map_plate_to_source(roi, &plate);

    std::array<double, 9> homography{};
    bool two_line = false;
    if (!alpr::plate_rectification_transform(surface.width, surface.height, plate,
                                              &homography, &two_line, error)) {
      return false;
    }
    if (!launch_lpr_rectify_rgba(
            static_cast<const uint8_t*>(surface.dataPtr), surface.pitch,
            surface.width, surface.height, homography, two_line, lpr_.input(),
            stream_) || cudaStreamSynchronize(stream_) != cudaSuccess) {
      *error = "GPU plate rectification failed";
      return false;
    }
    std::vector<alpr::TensorView> lpr_outputs;
    if (!lpr_.infer(&lpr_outputs, error)) return false;
    if (lpr_outputs.size() != 1U) {
      lpr_.release();
      *error = "LPR engine returned an unexpected output count";
      return false;
    }
    alpr::OcrResult ocr;
    const bool ocr_decoded = alpr::decode_lpr_ctc(lpr_outputs.front(), &ocr, error);
    lpr_.release();
    if (!ocr_decoded) return false;
    const auto event = recognition_.observe(item.job, plate, ocr, timestamp_now());
    if (event.has_value() && handler_) handler_(*event);
    return true;
  }

  PipelineConfig config_;
  EngineSet engines_;
  EventHandler handler_;
  RecognitionTracker recognition_;
  InferContext plate_;
  InferContext lpr_;
  cudaStream_t stream_{nullptr};
  mutable std::mutex error_mutex_;
  std::string last_error_;
};

GpuInferenceExecutor::GpuInferenceExecutor(const PipelineConfig& config,
                                           const EngineSet& engines,
                                           EventHandler event_handler)
    : implementation_(std::make_unique<Implementation>(
          config, engines, std::move(event_handler))) {}

GpuInferenceExecutor::~GpuInferenceExecutor() = default;

bool GpuInferenceExecutor::initialize(std::string* error) {
  return implementation_->initialize(error);
}

size_t GpuInferenceExecutor::process(const std::vector<PlateWorkItem>& jobs) {
  return implementation_->process(jobs);
}

void GpuInferenceExecutor::mark_seen(uint64_t track_id, uint64_t frame_number) {
  implementation_->mark_seen(track_id, frame_number);
}

std::vector<uint64_t> GpuInferenceExecutor::expire(uint64_t frame_number) {
  return implementation_->expire(frame_number);
}

std::vector<uint64_t> GpuInferenceExecutor::clear() {
  return implementation_->clear();
}

std::string GpuInferenceExecutor::last_error() const {
  return implementation_->last_error();
}

}  // namespace traffic_alpr
