#include "alpr/event.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

extern "C" char* MbfsAlprCopyEventJson(const alpr::AlprEvent* event) {
  if (event == nullptr) return nullptr;
  const std::string json = alpr::event_json(*event);
  char* output = static_cast<char*>(std::malloc(json.size() + 1U));
  if (output == nullptr) return nullptr;
  std::memcpy(output, json.c_str(), json.size() + 1U);
  return output;
}

extern "C" void MbfsAlprFreePayload(char* payload) {
  std::free(payload);
}
