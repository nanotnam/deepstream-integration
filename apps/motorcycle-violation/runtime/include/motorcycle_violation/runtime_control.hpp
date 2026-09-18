#pragma once

#include "messaging/runtime.hpp"
#include "service_runtime/service_state.hpp"

namespace motorcycle_violation {

using ServiceState = service_runtime::ServiceState;
using ReconnectBackoff = messaging::ReconnectBackoff;
using PublishMessage = messaging::Message;
using PublishQueue = messaging::PublishQueue;

inline const char* service_state_name(ServiceState state) {
  return service_runtime::service_state_name(state);
}

}  // namespace motorcycle_violation
