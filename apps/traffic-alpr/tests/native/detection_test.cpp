#include "alpr/detection.hpp"
#include "test_support.hpp"

#include <cassert>
#include <cmath>
#include <string>
#include <vector>

int main() {
  alpr::Settings settings;
  const alpr::ImageTransform transform{1.0F, 1.0F, 0.0F, 0.0F, 100U, 100U};
  FloatTensor scores{"scores", {1, 3, 7}, std::vector<float>(21U, 0.0F)};
  FloatTensor boxes{"boxes", {1, 3, 4}, std::vector<float>(12U, 0.1F)};
  scores.values[3] = 0.9F;
  scores.values[7U + 3U] = 0.8F;
  std::vector<alpr::TensorView> outputs{scores.view(), boxes.view()};
  const std::vector<std::string> labels{"moto", "truck", "bike", "car",
                                         "pedestrian", "bus", "ba_gac"};
  std::vector<alpr::Detection> detections;
  std::string error;
  assert(alpr::decode_vehicle_scrfd(100U, 100U, transform, outputs, labels,
                                    settings, &detections, &error));
  assert(detections.size() == 1U);
  assert(detections[0].label == "car");
  assert(std::abs(detections[0].box.width - 0.1F) < 1e-5F);

  FloatTensor plate_scores{"plate_scores", {1, 1, 1, 2}, {5.0F, -5.0F}};
  FloatTensor plate_boxes{"plate_boxes", {1, 1, 1, 8},
                          {0.1F, 0.1F, 0.2F, 0.2F, 0.0F, 0.0F, 0.0F, 0.0F}};
  FloatTensor keypoints{"keypoints", {1, 1, 1, 20}, std::vector<float>(20U, 0.1F)};
  outputs = {plate_scores.view(), plate_boxes.view(), keypoints.view()};
  alpr::PlateDetection plate;
  bool found = false;
  assert(alpr::decode_plate_scrfd(100U, 100U, transform, outputs, settings,
                                  &plate, &found, &error));
  assert(found);

  assert(plate.detection.score > 0.99F);

  FloatTensor nchw_scores{"plate_scores", {1, 2, 1, 1}, {5.0F, -5.0F}};
  FloatTensor nchw_boxes{"plate_boxes", {1, 8, 1, 1},
                         {0.1F, 0.1F, 0.2F, 0.2F, 0.0F, 0.0F, 0.0F, 0.0F}};
  FloatTensor nchw_keypoints{"keypoints", {1, 20, 1, 1},
                             std::vector<float>(20U, 0.1F)};
  outputs = {nchw_scores.view(), nchw_boxes.view(), nchw_keypoints.view()};
  assert(alpr::decode_plate_scrfd(100U, 100U, transform, outputs, settings,
                                  &plate, &found, &error));
  assert(found);

  FloatTensor batch_scores{"plate_scores", {2, 2, 1, 1},
                           {5.0F, -5.0F, 5.0F, -5.0F}};
  FloatTensor batch_boxes{"plate_boxes", {2, 8, 1, 1}, {}};
  batch_boxes.values.insert(batch_boxes.values.end(), nchw_boxes.values.begin(),
                            nchw_boxes.values.end());
  batch_boxes.values.insert(batch_boxes.values.end(), nchw_boxes.values.begin(),
                            nchw_boxes.values.end());
  FloatTensor batch_keypoints{"keypoints", {2, 20, 1, 1},
                              std::vector<float>(40U, 0.1F)};
  outputs = {batch_scores.view(), batch_boxes.view(), batch_keypoints.view()};
  const std::vector<alpr::PlateDecodeContext> contexts{
      {100U, 100U, transform}, {100U, 100U, transform}};
  std::vector<std::optional<alpr::PlateDetection>> batch_plates;
  assert(alpr::decode_plate_scrfd_batch(contexts, outputs, settings, &batch_plates,
                                        &error));
  assert(batch_plates.size() == 2U);
  assert(batch_plates[0].has_value() && batch_plates[1].has_value());

  const alpr::Box region{0.25F, 0.25F, 0.5F, 0.5F};
  alpr::map_plate_to_source(region, &plate);
  assert(plate.detection.box.x >= 0.25F);

  FloatTensor invalid{"invalid", {1, 3, 7}, std::vector<float>(1U, 0.0F)};
  outputs = {invalid.view(), boxes.view()};
  assert(!alpr::decode_vehicle_scrfd(100U, 100U, transform, outputs, labels,
                                     settings, &detections, &error));
  assert(!error.empty());
  return 0;
}
