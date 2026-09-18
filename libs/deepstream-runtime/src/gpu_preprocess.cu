#include "gpu_preprocess.hpp"

#include <cuda_runtime_api.h>

#include <algorithm>
#include <cmath>

namespace deepstream_runtime {
namespace {

constexpr int kPlateWidth = 640;
constexpr int kPlateHeight = 640;
constexpr int kLprWidth = 156;
constexpr int kLprHeight = 32;

__device__ float channel(const uint8_t* image, size_t pitch, uint32_t width,
                         uint32_t height, float x, float y, int component) {
  x = fminf(fmaxf(x, 0.0F), static_cast<float>(width - 1U));
  y = fminf(fmaxf(y, 0.0F), static_cast<float>(height - 1U));
  const uint32_t x0 = static_cast<uint32_t>(floorf(x));
  const uint32_t y0 = static_cast<uint32_t>(floorf(y));
  const uint32_t x1 = min(x0 + 1U, width - 1U);
  const uint32_t y1 = min(y0 + 1U, height - 1U);
  const float fx = x - static_cast<float>(x0);
  const float fy = y - static_cast<float>(y0);
  const float p00 = image[static_cast<size_t>(y0) * pitch + x0 * 4U + component];
  const float p10 = image[static_cast<size_t>(y0) * pitch + x1 * 4U + component];
  const float p01 = image[static_cast<size_t>(y1) * pitch + x0 * 4U + component];
  const float p11 = image[static_cast<size_t>(y1) * pitch + x1 * 4U + component];
  const float top = p00 * (1.0F - fx) + p10 * fx;
  const float bottom = p01 * (1.0F - fx) + p11 * fx;
  return top * (1.0F - fy) + bottom * fy;
}

__global__ void plate_kernel(const uint8_t* rgba, size_t pitch, uint32_t width,
                             uint32_t height, float roi_x, float roi_y,
                             float roi_width, float roi_height, float scale,
                             float* output) {
  const int x = blockIdx.x * blockDim.x + threadIdx.x;
  const int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= kPlateWidth || y >= kPlateHeight) return;
  const float resized_width = roi_width * scale;
  const float resized_height = roi_height * scale;
  float rgb[3] = {16.0F, 16.0F, 16.0F};
  if (static_cast<float>(x) < resized_width &&
      static_cast<float>(y) < resized_height) {
    const float source_x = roi_x + (static_cast<float>(x) + 0.5F) / scale - 0.5F;
    const float source_y = roi_y + (static_cast<float>(y) + 0.5F) / scale - 0.5F;
    for (int component = 0; component < 3; ++component) {
      rgb[component] = channel(rgba, pitch, width, height, source_x, source_y,
                               component);
    }
  }
  const size_t plane = static_cast<size_t>(kPlateWidth) * kPlateHeight;
  const size_t offset = static_cast<size_t>(y) * kPlateWidth + x;
  for (int component = 0; component < 3; ++component) {
    output[static_cast<size_t>(component) * plane + offset] =
        (rgb[component] - 16.0F) * 0.8588F;
  }
}

__global__ void lpr_kernel(const uint8_t* rgba, size_t pitch, uint32_t width,
                           uint32_t height, const double* transform,
                           bool two_line, float* output) {
  const int x = blockIdx.x * blockDim.x + threadIdx.x;
  const int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= kLprWidth || y >= kLprHeight) return;
  float warped_x = static_cast<float>(x);
  float warped_y = static_cast<float>(y);
  if (two_line) {
    const float arranged_x = (warped_x + 0.5F) * 2.0F - 0.5F;
    warped_y = (warped_y + 0.5F) * 0.5F - 0.5F;
    const bool lower = arranged_x >= kLprWidth;
    warped_x = lower ? arranged_x - kLprWidth : arranged_x;
    if (lower) warped_y += kLprHeight / 2.0F;
  }
  const double denominator = transform[6] * warped_x +
                             transform[7] * warped_y + transform[8];
  float source_x = 0.0F;
  float source_y = 0.0F;
  if (fabs(denominator) >= 1e-9) {
    source_x = static_cast<float>((transform[0] * warped_x +
        transform[1] * warped_y + transform[2]) / denominator);
    source_y = static_cast<float>((transform[3] * warped_x +
        transform[4] * warped_y + transform[5]) / denominator);
  }
  const size_t offset = (static_cast<size_t>(y) * kLprWidth + x) * 3U;
  output[offset] = channel(rgba, pitch, width, height, source_x, source_y, 2);
  output[offset + 1U] = channel(rgba, pitch, width, height, source_x, source_y, 1);
  output[offset + 2U] = channel(rgba, pitch, width, height, source_x, source_y, 0);
}

}  // namespace

bool launch_plate_preprocess_rgba(const uint8_t* rgba, size_t pitch,
                                  uint32_t width, uint32_t height,
                                  float roi_x, float roi_y, float roi_width,
                                  float roi_height, float* output,
                                  void* stream) {
  if (rgba == nullptr || output == nullptr || width == 0U || height == 0U ||
      roi_width <= 1.0F || roi_height <= 1.0F) return false;
  const float scale = std::min(kPlateWidth / roi_width, kPlateHeight / roi_height);
  const dim3 threads(16U, 16U);
  const dim3 blocks((kPlateWidth + threads.x - 1U) / threads.x,
                    (kPlateHeight + threads.y - 1U) / threads.y);
  plate_kernel<<<blocks, threads, 0U, static_cast<cudaStream_t>(stream)>>>(
      rgba, pitch, width, height, roi_x, roi_y, roi_width, roi_height, scale,
      output);
  return cudaGetLastError() == cudaSuccess;
}

bool launch_lpr_rectify_rgba(const uint8_t* rgba, size_t pitch,
                             uint32_t width, uint32_t height,
                             const std::array<double, 9>& transform,
                             bool two_line, float* output, void* stream) {
  if (rgba == nullptr || output == nullptr || width == 0U || height == 0U) {
    return false;
  }
  double* device_transform = nullptr;
  if (cudaMallocAsync(&device_transform, sizeof(double) * transform.size(),
                      static_cast<cudaStream_t>(stream)) != cudaSuccess) return false;
  if (cudaMemcpyAsync(device_transform, transform.data(),
                      sizeof(double) * transform.size(), cudaMemcpyHostToDevice,
                      static_cast<cudaStream_t>(stream)) != cudaSuccess) {
    cudaFreeAsync(device_transform, static_cast<cudaStream_t>(stream));
    return false;
  }
  const dim3 threads(16U, 8U);
  const dim3 blocks((kLprWidth + threads.x - 1U) / threads.x,
                    (kLprHeight + threads.y - 1U) / threads.y);
  lpr_kernel<<<blocks, threads, 0U, static_cast<cudaStream_t>(stream)>>>(
      rgba, pitch, width, height, device_transform, two_line, output);
  const bool success = cudaGetLastError() == cudaSuccess;
  cudaFreeAsync(device_transform, static_cast<cudaStream_t>(stream));
  return success;
}

}  // namespace deepstream_runtime
