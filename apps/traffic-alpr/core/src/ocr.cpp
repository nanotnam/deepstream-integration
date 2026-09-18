#include "alpr/ocr.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <regex>

namespace alpr {
namespace {

float selected_probability(const TensorView& tensor, size_t time_step,
                           size_t selected) {
  float maximum = tensor_value(tensor, time_step * kLprClasses);
  for (size_t class_index = 1U; class_index < kLprClasses; ++class_index) {
    maximum = std::max(maximum,
                       tensor_value(tensor, time_step * kLprClasses + class_index));
  }
  float denominator = 0.0F;
  float numerator = 0.0F;
  for (size_t class_index = 0U; class_index < kLprClasses; ++class_index) {
    const float value = std::exp(
        tensor_value(tensor, time_step * kLprClasses + class_index) - maximum);
    denominator += value;
    if (class_index == selected) numerator = value;
  }
  return denominator > 0.0F ? numerator / denominator : 0.0F;
}

}  // namespace

bool decode_lpr_ctc(const TensorView& tensor, OcrResult* result,
                    std::string* error) {
  if (tensor.shape != std::vector<int64_t>({1, 39, 35})) {
    *error = "LPR output must have shape [1,39,35]";
    return false;
  }
  size_t count = 0;
  if (!validate_tensor(tensor, &count, error)) return false;
  if (count != 39U * kLprClasses) {
    *error = "LPR output element count is invalid";
    return false;
  }
  result->text.clear();
  result->character_confidences.clear();
  int previous = -1;
  for (size_t time_step = 0U; time_step < 39U; ++time_step) {
    size_t selected = 0U;
    float selected_value = tensor_value(tensor, time_step * kLprClasses);
    for (size_t class_index = 1U; class_index < kLprClasses; ++class_index) {
      const float value = tensor_value(tensor, time_step * kLprClasses + class_index);
      if (value > selected_value) {
        selected = class_index;
        selected_value = value;
      }
    }
    if (selected != kLprBlankIndex && static_cast<int>(selected) != previous) {
      result->text.push_back(kLprAlphabet[selected]);
      result->character_confidences.push_back(
          selected_probability(tensor, time_step, selected));
    }
    previous = static_cast<int>(selected);
  }
  result->confidence = result->character_confidences.empty()
                           ? 0.0F
                           : std::accumulate(result->character_confidences.begin(),
                                             result->character_confidences.end(), 0.0F) /
                                 result->character_confidences.size();
  return true;
}

bool decode_lpr_ctc_batch(const TensorView& tensor,
                          std::vector<OcrResult>* results,
                          std::string* error) {
  results->clear();
  if (tensor.shape.size() != 3U || tensor.shape[0] <= 0 ||
      tensor.shape[1] != 39 || tensor.shape[2] != 35) {
    *error = "batched LPR output must have shape [N,39,35]";
    return false;
  }
  results->reserve(static_cast<size_t>(tensor.shape[0]));
  for (size_t batch_index = 0U;
       batch_index < static_cast<size_t>(tensor.shape[0]); ++batch_index) {
    TensorView slice;
    if (!tensor_batch_slice(tensor, batch_index, &slice, error)) return false;
    OcrResult result;
    if (!decode_lpr_ctc(slice, &result, error)) return false;
    results->push_back(std::move(result));
  }
  return true;
}

std::string normalize_plate(std::string text) {
  std::string result;
  for (unsigned char value : text) {
    if (value >= 'a' && value <= 'z') value -= static_cast<unsigned char>('a' - 'A');
    if ((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9')) {
      result.push_back(static_cast<char>(value));
    }
  }
  return result;
}

std::optional<std::string> format_vietnam_plate(const std::string& text,
                                                bool prefer_car) {
  const std::string normalized = normalize_plate(text);
  static const std::regex motorcycle("^([0-9]{2})([A-Z])([A-Z0-9])([0-9]{4,5})$");
  static const std::regex car("^([0-9]{2})([A-Z])([0-9]{5})$");
  std::smatch match;
  if (prefer_car && std::regex_match(normalized, match, car)) {
    return match[1].str() + match[2].str() + " " + match[3].str();
  }
  if (std::regex_match(normalized, match, motorcycle)) {
    return match[1].str() + match[2].str() + match[3].str() + " " + match[4].str();
  }
  if (std::regex_match(normalized, match, car)) {
    return match[1].str() + match[2].str() + " " + match[3].str();
  }
  return std::nullopt;
}

PlateVote::PlateVote(size_t expected_length, size_t maximum_values,
                     float lock_confidence)
    : expected_length_(expected_length), maximum_values_(maximum_values),
      lock_confidence_(lock_confidence), history_(expected_length),
      locked_(expected_length, '\0') {}

std::pair<char, float> PlateVote::best(size_t index) const {
  if (index >= history_.size() || history_[index].empty()) return {'\0', 0.0F};
  if (locked_[index] != '\0') return {locked_[index], 1.0F};
  std::unordered_map<char, float> scores;
  std::unordered_map<char, size_t> counts;
  for (const auto& [character, confidence] : history_[index]) {
    scores[character] += confidence;
    ++counts[character];
  }
  char selected = '\0';
  float selected_score = -1.0F;
  for (const auto& [character, score] : scores) {
    if (score > selected_score || (score == selected_score && character < selected)) {
      selected = character;
      selected_score = score;
    }
  }
  return {selected, selected_score / static_cast<float>(counts[selected])};
}

std::optional<std::string> PlateVote::add(const OcrResult& result, float quality) {
  const std::string normalized = normalize_plate(result.text);
  if ((normalized.size() != 8U && normalized.size() != 9U) ||
      result.character_confidences.size() != normalized.size()) return std::nullopt;
  ++observations_;
  observed_length_ = normalized.size();
  if (quality > best_quality_) {
    best_quality_ = quality;
    best_text_ = normalized;
    best_confidences_ = result.character_confidences;
  }
  for (size_t index = 0U; index < normalized.size() && index < expected_length_; ++index) {
    if (locked_[index] != '\0') continue;
    auto& values = history_[index];
    values.emplace_back(normalized[index], result.character_confidences[index]);
    if (values.size() > maximum_values_) values.erase(values.begin());
    const auto [character, confidence] = best(index);
    if (values.size() >= maximum_values_ && confidence > lock_confidence_) {
      locked_[index] = character;
    }
  }
  return format_vietnam_plate(best_text_);
}

std::optional<std::string> PlateVote::finalize(size_t minimum_observations,
                                               bool prefer_car) const {
  if (observations_ < minimum_observations) return std::nullopt;
  if (const auto best_text = format_vietnam_plate(best_text_, prefer_car)) return best_text;
  const std::string candidate = partial();
  if (candidate.empty() || candidate.find('_') != std::string::npos) return std::nullopt;
  return format_vietnam_plate(candidate, prefer_car);
}

std::string PlateVote::partial() const {
  std::string result;
  const size_t length = observed_length_ == 0U ? expected_length_ : observed_length_;
  bool decided = false;
  for (size_t index = 0U; index < length; ++index) {
    const auto [character, confidence] = best(index);
    static_cast<void>(confidence);
    decided = decided || character != '\0';
    result.push_back(character == '\0' ? '_' : character);
  }
  return decided ? result : std::string();
}

float PlateVote::confidence() const {
  if (best_confidences_.empty()) return 0.0F;
  return std::accumulate(best_confidences_.begin(), best_confidences_.end(), 0.0F) /
         best_confidences_.size();
}

size_t PlateVote::observations() const { return observations_; }

bool PlateRegistry::claim(uint64_t track_id, const std::string& plate,
                          uint64_t timestamp_us, uint64_t cooldown_us) {
  const std::string key = normalize_plate(plate);
  if (key.size() < 7U) return false;
  if (cooldown_us == 0U) return true;
  const auto [entry, inserted] = owners_.emplace(key, Owner{track_id, timestamp_us});
  if (inserted || entry->second.track_id == track_id) {
    entry->second = Owner{track_id, timestamp_us};
    return true;
  }
  if (timestamp_us >= entry->second.timestamp_us &&
      timestamp_us - entry->second.timestamp_us < cooldown_us) return false;
  entry->second = Owner{track_id, timestamp_us};
  return true;
}

}  // namespace alpr
