#pragma once

#include "alpr/tensor.hpp"
#include "alpr/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace alpr {

inline constexpr char kLprAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ0123456789-";
inline constexpr size_t kLprClasses = 35;
inline constexpr size_t kLprBlankIndex = 34;

bool decode_lpr_ctc(const TensorView& tensor, OcrResult* result,
                    std::string* error);
bool decode_lpr_ctc_batch(const TensorView& tensor,
                          std::vector<OcrResult>* results,
                          std::string* error);
std::string normalize_plate(std::string text);
std::optional<std::string> format_vietnam_plate(const std::string& text,
                                                bool prefer_car = false);

class PlateVote {
 public:
  PlateVote(size_t expected_length, size_t maximum_values, float lock_confidence);
  std::optional<std::string> add(const OcrResult& result, float quality);
  std::optional<std::string> finalize(size_t minimum_observations,
                                      bool prefer_car = false) const;
  std::string partial() const;
  float confidence() const;
  size_t observations() const;

 private:
  std::pair<char, float> best(size_t index) const;
  size_t expected_length_;
  size_t maximum_values_;
  float lock_confidence_;
  size_t observations_{0};
  size_t observed_length_{0};
  float best_quality_{-1.0F};
  std::string best_text_;
  std::vector<float> best_confidences_;
  std::vector<std::vector<std::pair<char, float>>> history_;
  std::vector<char> locked_;
};

class PlateRegistry {
 public:
  bool claim(uint64_t track_id, const std::string& plate, uint64_t timestamp_us,
             uint64_t cooldown_us);

 private:
  struct Owner { uint64_t track_id; uint64_t timestamp_us; };
  std::unordered_map<std::string, Owner> owners_;
};

}  // namespace alpr
