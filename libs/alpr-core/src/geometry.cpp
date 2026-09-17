#include "alpr/geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace alpr {
namespace {

constexpr int kOutputWidth = 156;
constexpr int kOutputHeight = 32;

float distance(const Point& lhs, const Point& rhs) {
  return std::hypot(rhs.x - lhs.x, rhs.y - lhs.y);
}

bool solve(std::array<std::array<double, 9>, 8>* matrix,
           std::array<double, 8>* solution) {
  for (size_t column = 0; column < 8U; ++column) {
    size_t pivot = column;
    for (size_t row = column + 1U; row < 8U; ++row) {
      if (std::abs((*matrix)[row][column]) >
          std::abs((*matrix)[pivot][column])) pivot = row;
    }
    if (std::abs((*matrix)[pivot][column]) < 1e-9) return false;
    std::swap((*matrix)[pivot], (*matrix)[column]);
    const double divisor = (*matrix)[column][column];
    for (size_t item = column; item < 9U; ++item) (*matrix)[column][item] /= divisor;
    for (size_t row = 0; row < 8U; ++row) {
      if (row == column) continue;
      const double factor = (*matrix)[row][column];
      for (size_t item = column; item < 9U; ++item) {
        (*matrix)[row][item] -= factor * (*matrix)[column][item];
      }
    }
  }
  for (size_t row = 0; row < 8U; ++row) (*solution)[row] = (*matrix)[row][8];
  return true;
}

bool homography(const std::array<Point, 4>& destination,
                const std::array<Point, 4>& source,
                std::array<double, 9>* result) {
  std::array<std::array<double, 9>, 8> matrix{};
  for (size_t index = 0; index < 4U; ++index) {
    const double x = destination[index].x;
    const double y = destination[index].y;
    const double u = source[index].x;
    const double v = source[index].y;
    matrix[index * 2U] = {x, y, 1.0, 0.0, 0.0, 0.0, -u * x, -u * y, u};
    matrix[index * 2U + 1U] = {0.0, 0.0, 0.0, x, y, 1.0, -v * x, -v * y, v};
  }
  std::array<double, 8> solution{};
  if (!solve(&matrix, &solution)) return false;
  *result = {solution[0], solution[1], solution[2], solution[3], solution[4],
             solution[5], solution[6], solution[7], 1.0};
  return true;
}

std::array<float, 3> pixel_rgb(const ImageView& image, uint32_t x, uint32_t y) {
  const size_t offset = static_cast<size_t>(y) * image.stride +
                        static_cast<size_t>(x) * 3U;
  if (image.format == PixelFormat::kRgb) {
    return {static_cast<float>(image.data[offset]),
            static_cast<float>(image.data[offset + 1U]),
            static_cast<float>(image.data[offset + 2U])};
  }
  return {static_cast<float>(image.data[offset + 2U]),
          static_cast<float>(image.data[offset + 1U]),
          static_cast<float>(image.data[offset])};
}

std::array<float, 3> sample_rgb(const ImageView& image, float x, float y) {
  x = std::clamp(x, 0.0F, static_cast<float>(image.width - 1U));
  y = std::clamp(y, 0.0F, static_cast<float>(image.height - 1U));
  const uint32_t x0 = static_cast<uint32_t>(std::floor(x));
  const uint32_t y0 = static_cast<uint32_t>(std::floor(y));
  const uint32_t x1 = std::min(image.width - 1U, x0 + 1U);
  const uint32_t y1 = std::min(image.height - 1U, y0 + 1U);
  const float fx = x - static_cast<float>(x0);
  const float fy = y - static_cast<float>(y0);
  const auto p00 = pixel_rgb(image, x0, y0);
  const auto p10 = pixel_rgb(image, x1, y0);
  const auto p01 = pixel_rgb(image, x0, y1);
  const auto p11 = pixel_rgb(image, x1, y1);
  std::array<float, 3> output{};
  for (size_t channel = 0; channel < output.size(); ++channel) {
    const float top = p00[channel] * (1.0F - fx) + p10[channel] * fx;
    const float bottom = p01[channel] * (1.0F - fx) + p11[channel] * fx;
    output[channel] = top * (1.0F - fy) + bottom * fy;
  }
  return output;
}

std::array<float, 3> warped_sample(const ImageView& image,
                                   const std::array<double, 9>& transform,
                                   float x, float y) {
  const double denominator = transform[6] * x + transform[7] * y + transform[8];
  if (std::abs(denominator) < 1e-9) return {};
  const float source_x = static_cast<float>(
      (transform[0] * x + transform[1] * y + transform[2]) / denominator);
  const float source_y = static_cast<float>(
      (transform[3] * x + transform[4] * y + transform[5]) / denominator);
  return sample_rgb(image, source_x, source_y);
}

}  // namespace

bool valid_image(const ImageView& image, std::string* error) {
  if (image.width == 0U || image.height == 0U || image.data == nullptr ||
      image.stride < static_cast<size_t>(image.width) * 3U ||
      image.size < image.stride * image.height) {
    *error = "image must be a valid packed RGB/BGR buffer";
    return false;
  }
  return true;
}

bool valid_plate_keypoints(const std::array<Point, 5>& keypoints) {
  for (const Point& point : keypoints) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
  }
  return distance(keypoints[0], keypoints[1]) > 1e-4F &&
         distance(keypoints[0], keypoints[3]) > 1e-4F;
}

bool is_two_line_plate(const std::array<Point, 5>& keypoints) {
  const float height = distance(keypoints[0], keypoints[3]);
  return height > 1e-3F && distance(keypoints[0], keypoints[1]) / height < 2.3F;
}

bool rectify_plate_bgr(const ImageView& image, const PlateDetection& plate,
                       std::vector<uint8_t>* output, std::string* error) {
  if (!valid_image(image, error)) return false;
  std::array<Point, 4> source{};
  const bool use_keypoints = valid_plate_keypoints(plate.keypoints);
  if (use_keypoints) {
    source = {plate.keypoints[0], plate.keypoints[1], plate.keypoints[4],
              plate.keypoints[3]};
    for (Point& point : source) {
      point.x *= image.width;
      point.y *= image.height;
    }
  } else {
    const Box& box = plate.detection.box;
    const float left = box.x * image.width;
    const float top = box.y * image.height;
    const float right = (box.x + box.width) * image.width;
    const float bottom = (box.y + box.height) * image.height;
    if (right <= left + 2.0F || bottom <= top + 2.0F) {
      *error = "plate keypoints and fallback box are invalid";
      return false;
    }
    source = {{{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
  }
  const std::array<Point, 4> destination{{{0.0F, 0.0F},
                                          {kOutputWidth - 1.0F, 0.0F},
                                          {kOutputWidth - 1.0F, kOutputHeight - 1.0F},
                                          {0.0F, kOutputHeight - 1.0F}}};
  std::array<double, 9> transform{};
  if (!homography(destination, source, &transform)) {
    *error = "plate homography is singular";
    return false;
  }
  output->assign(static_cast<size_t>(kOutputWidth) * kOutputHeight * 3U, 0U);
  const bool two_line = use_keypoints && is_two_line_plate(plate.keypoints);
  for (int y = 0; y < kOutputHeight; ++y) {
    for (int x = 0; x < kOutputWidth; ++x) {
      std::array<float, 3> pixel{};
      if (!two_line) {
        pixel = warped_sample(image, transform, static_cast<float>(x),
                              static_cast<float>(y));
      } else {
        const float arranged_x = (static_cast<float>(x) + 0.5F) * 2.0F - 0.5F;
        const float arranged_y = (static_cast<float>(y) + 0.5F) * 0.5F - 0.5F;
        const bool lower = arranged_x >= kOutputWidth;
        pixel = warped_sample(image, transform,
                              lower ? arranged_x - kOutputWidth : arranged_x,
                              arranged_y + (lower ? kOutputHeight / 2.0F : 0.0F));
      }
      const size_t offset = (static_cast<size_t>(y) * kOutputWidth +
                             static_cast<size_t>(x)) * 3U;
      (*output)[offset] = static_cast<uint8_t>(
          std::clamp(std::lround(pixel[2]), 0L, 255L));
      (*output)[offset + 1U] = static_cast<uint8_t>(
          std::clamp(std::lround(pixel[1]), 0L, 255L));
      (*output)[offset + 2U] = static_cast<uint8_t>(
          std::clamp(std::lround(pixel[0]), 0L, 255L));
    }
  }
  return true;
}

}  // namespace alpr

