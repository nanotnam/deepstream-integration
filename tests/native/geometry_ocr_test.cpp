#include "alpr/geometry.hpp"
#include "alpr/ocr.hpp"
#include "test_support.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
  std::vector<uint8_t> pixels(200U * 100U * 3U);
  for (size_t index = 0U; index < pixels.size(); ++index) {
    pixels[index] = static_cast<uint8_t>(index % 251U);
  }
  const alpr::ImageView image{200U, 100U, 600U, alpr::PixelFormat::kRgb,
                              pixels.data(), pixels.size()};
  alpr::PlateDetection plate;
  plate.detection.box = {0.1F, 0.2F, 0.7F, 0.4F};
  std::vector<uint8_t> rectified;
  std::string error;
  assert(alpr::rectify_plate_bgr(image, plate, &rectified, &error));
  assert(rectified.size() == 156U * 32U * 3U);

  plate.keypoints = {{{0.1F, 0.2F}, {0.4F, 0.2F}, {0.25F, 0.4F},
                      {0.1F, 0.7F}, {0.4F, 0.7F}}};
  assert(alpr::valid_plate_keypoints(plate.keypoints));
  assert(alpr::is_two_line_plate(plate.keypoints));
  assert(alpr::rectify_plate_bgr(image, plate, &rectified, &error));

  FloatTensor logits{"lpr", {1, 39, 35}, std::vector<float>(39U * 35U, -10.0F)};
  for (size_t step = 0U; step < 39U; ++step) {
    logits.values[step * 35U + alpr::kLprBlankIndex] = 10.0F;
  }
  const std::string expected = "89AA15689";
  size_t step = 0U;
  char previous = '\0';
  for (const char character : expected) {
    if (character == previous) ++step;
    logits.values[step * 35U + alpr::kLprBlankIndex] = -10.0F;
    logits.values[step * 35U + alphabet_index(character)] = 10.0F;
    previous = character;
    ++step;
  }
  alpr::OcrResult result;
  assert(alpr::decode_lpr_ctc(logits.view(), &result, &error));
  assert(result.text == expected);
  assert(result.confidence > 0.99F);
  assert(alpr::normalize_plate("89-aa 15689") == expected);
  assert(alpr::format_vietnam_plate(expected).value() == "89AA 15689");

  alpr::PlateVote vote(9U, 3U, 0.8F);
  assert(vote.add(result, 0.8F).has_value());
  assert(vote.add(result, 0.9F).has_value());
  assert(!vote.finalize(3U).has_value());
  assert(vote.add(result, 1.0F).has_value());
  assert(vote.finalize(3U).value() == "89AA 15689");
  return 0;
}

