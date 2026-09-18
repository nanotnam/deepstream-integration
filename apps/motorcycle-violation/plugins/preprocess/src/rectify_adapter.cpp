#include "alpr_deepstream/rectify_adapter.hpp"

#include "alpr/geometry.hpp"

#include <algorithm>
#include <string>
#include <vector>

extern "C" bool MbfsAlprRectifyPackedImage(
    const uint8_t* source, size_t source_size, uint32_t width, uint32_t height,
    size_t stride, bool source_is_bgr, const alpr::Box* box,
    const std::array<alpr::Point, 5>* keypoints, uint8_t* destination_bgr,
    size_t destination_size) {
  constexpr size_t kExpectedBytes = 156U * 32U * 3U;
  if (box == nullptr || keypoints == nullptr || destination_bgr == nullptr ||
      destination_size < kExpectedBytes) return false;
  const alpr::ImageView image{width, height, stride,
      source_is_bgr ? alpr::PixelFormat::kBgr : alpr::PixelFormat::kRgb,
      source, source_size};
  alpr::PlateDetection plate;
  plate.detection.box = *box;
  plate.keypoints = *keypoints;
  std::vector<uint8_t> output;
  std::string error;
  if (!alpr::rectify_plate_bgr(image, plate, &output, &error)) return false;
  std::copy(output.begin(), output.end(), destination_bgr);
  return true;
}
