#include "alpr/tensor.hpp"

#include <cstring>
#include <limits>

namespace alpr {

bool validate_tensor(const TensorView& tensor, size_t* element_count,
                     std::string* error) {
  if (tensor.data == nullptr || tensor.shape.empty()) {
    *error = "tensor data and shape are required";
    return false;
  }
  size_t count = 1;
  for (const int64_t dimension : tensor.shape) {
    if (dimension <= 0 || count > std::numeric_limits<size_t>::max() /
                                      static_cast<size_t>(dimension)) {
      *error = "tensor has an invalid shape";
      return false;
    }
    count *= static_cast<size_t>(dimension);
  }
  const size_t element_size = tensor.data_type == DataType::kFloat32 ? sizeof(float) : 1U;
  if (count > std::numeric_limits<size_t>::max() / element_size ||
      tensor.byte_size != count * element_size) {
    *error = "tensor byte size does not match its shape";
    return false;
  }
  if (tensor.data_type != DataType::kFloat32 && tensor.quantization.scale <= 0.0F) {
    *error = "quantized tensor requires a positive scale";
    return false;
  }
  *element_count = count;
  return true;
}

float tensor_value(const TensorView& tensor, size_t index) {
  if (tensor.data_type == DataType::kFloat32) {
    float value = 0.0F;
    std::memcpy(&value, static_cast<const uint8_t*>(tensor.data) + index * sizeof(float),
                sizeof(float));
    return value;
  }
  const int32_t value = tensor.data_type == DataType::kInt8
                            ? static_cast<const int8_t*>(tensor.data)[index]
                            : static_cast<const uint8_t*>(tensor.data)[index];
  return (value - tensor.quantization.zero_point) * tensor.quantization.scale;
}

bool tensor_batch_slice(const TensorView& tensor, size_t batch_index,
                        TensorView* slice, std::string* error) {
  size_t element_count = 0U;
  if (!validate_tensor(tensor, &element_count, error)) return false;
  const size_t batch_size = static_cast<size_t>(tensor.shape.front());
  if (batch_index >= batch_size || element_count % batch_size != 0U) {
    *error = "tensor batch slice is out of range";
    return false;
  }
  const size_t element_size = tensor.data_type == DataType::kFloat32 ? sizeof(float) : 1U;
  const size_t sample_bytes = element_count / batch_size * element_size;
  *slice = tensor;
  slice->shape.front() = 1;
  slice->data = static_cast<const uint8_t*>(tensor.data) + batch_index * sample_bytes;
  slice->byte_size = sample_bytes;
  return true;
}

}  // namespace alpr
