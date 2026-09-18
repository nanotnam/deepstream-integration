#pragma once

#include "alpr/ocr.hpp"
#include "alpr/tensor.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct FloatTensor {
  std::string name;
  std::vector<int64_t> shape;
  std::vector<float> values;

  alpr::TensorView view() const {
    return {name, shape, alpr::DataType::kFloat32, {}, values.data(),
            values.size() * sizeof(float)};
  }
};

inline size_t alphabet_index(char character) {
  const std::string alphabet = alpr::kLprAlphabet;
  const size_t index = alphabet.find(character);
  assert(index != std::string::npos);
  return index;
}
