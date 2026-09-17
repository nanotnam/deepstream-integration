#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace alpr {

enum class DataType { kFloat32, kInt8, kUint8 };

struct Quantization {
  float scale{0.0F};
  int32_t zero_point{0};
};

struct TensorView {
  std::string name;
  std::vector<int64_t> shape;
  DataType data_type{DataType::kFloat32};
  Quantization quantization;
  const void* data{nullptr};
  size_t byte_size{0};
};

bool validate_tensor(const TensorView& tensor, size_t* element_count,
                     std::string* error);
float tensor_value(const TensorView& tensor, size_t index);
bool tensor_batch_slice(const TensorView& tensor, size_t batch_index,
                        TensorView* slice, std::string* error);

}  // namespace alpr
