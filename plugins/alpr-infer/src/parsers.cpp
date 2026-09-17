#include "alpr/detection.hpp"
#include "alpr_deepstream/parser_contract.hpp"

#include <nvdsinfer_custom_impl.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

bool views(const std::vector<NvDsInferLayerInfo>& layers,
           std::vector<alpr::TensorView>* output) {
  output->clear();
  output->reserve(layers.size());
  for (const NvDsInferLayerInfo& layer : layers) {
    alpr::TensorView tensor;
    tensor.name = layer.layerName == nullptr ? "" : layer.layerName;
    size_t count = 1U;
    for (unsigned index = 0U; index < layer.inferDims.numDims; ++index) {
      const int dimension = layer.inferDims.d[index];
      if (dimension <= 0) return false;
      tensor.shape.push_back(dimension);
      count *= static_cast<size_t>(dimension);
    }
    if (tensor.shape.empty() || tensor.shape.front() != 1) {
      tensor.shape.insert(tensor.shape.begin(), 1);
    }
    if (layer.dataType != FLOAT) return false;
    tensor.data_type = alpr::DataType::kFloat32;
    tensor.data = layer.buffer;
    tensor.byte_size = count * sizeof(float);
    output->push_back(std::move(tensor));
  }
  return true;
}

alpr::ImageTransform identity_transform(const NvDsInferNetworkInfo& network) {
  return {1.0F, 1.0F, 0.0F, 0.0F, network.width, network.height};
}

void append(const std::vector<alpr::Detection>& detections,
            uint32_t width, uint32_t height,
            std::vector<NvDsInferObjectDetectionInfo>* objects) {
  for (const alpr::Detection& detection : detections) {
    NvDsInferObjectDetectionInfo object{};
    object.classId = static_cast<unsigned>(detection.class_id);
    object.detectionConfidence = detection.score;
    object.left = detection.box.x * width;
    object.top = detection.box.y * height;
    object.width = detection.box.width * width;
    object.height = detection.box.height * height;
    objects->push_back(object);
  }
}

}  // namespace

extern "C" bool NvDsInferParseMbfsVehicle(
    const std::vector<NvDsInferLayerInfo>& layers,
    const NvDsInferNetworkInfo& network,
    const NvDsInferParseDetectionParams& parameters,
    std::vector<NvDsInferObjectDetectionInfo>& objects) {
  std::vector<alpr::TensorView> tensors;
  if (!views(layers, &tensors) || parameters.numClassesConfigured != 7U) return false;
  std::vector<std::string> labels{"moto", "truck", "bike", "car",
                                  "pedestrian", "bus", "ba_gac"};
  alpr::Settings settings;
  if (!parameters.perClassPreclusterThreshold.empty()) {
    settings.vehicle_threshold = parameters.perClassPreclusterThreshold.front();
  }
  std::vector<alpr::Detection> detections;
  std::string error;
  if (!alpr::decode_vehicle_scrfd(network.width, network.height,
                                  identity_transform(network), tensors, labels,
                                  settings, &detections, &error)) return false;
  append(detections, network.width, network.height, &objects);
  return true;
}
CHECK_CUSTOM_PARSE_FUNC_PROTOTYPE(NvDsInferParseMbfsVehicle);

extern "C" bool NvDsInferParseMbfsPlate(
    const std::vector<NvDsInferLayerInfo>& layers,
    const NvDsInferNetworkInfo& network,
    const NvDsInferParseDetectionParams& parameters,
    std::vector<NvDsInferObjectDetectionInfo>& objects) {
  std::vector<alpr::TensorView> tensors;
  if (!views(layers, &tensors)) return false;
  alpr::Settings settings;
  if (!parameters.perClassPreclusterThreshold.empty()) {
    settings.plate_threshold = parameters.perClassPreclusterThreshold.front();
  }
  alpr::PlateDetection plate;
  bool found = false;
  std::string error;
  if (!alpr::decode_plate_scrfd(network.width, network.height,
                                identity_transform(network), tensors, settings,
                                &plate, &found, &error)) return false;
  if (found) append({plate.detection}, network.width, network.height, &objects);
  return true;
}
CHECK_CUSTOM_PARSE_FUNC_PROTOTYPE(NvDsInferParseMbfsPlate);

