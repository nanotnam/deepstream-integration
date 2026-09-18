#include "service_runtime/service_state.hpp"

namespace service_runtime {

const char* service_state_name(ServiceState state) {
  switch (state) {
    case ServiceState::kStarting: return "starting";
    case ServiceState::kRunning: return "running";
    case ServiceState::kDegraded: return "degraded";
    case ServiceState::kReconnecting: return "reconnecting";
    case ServiceState::kStopping: return "stopping";
  }
  return "degraded";
}

}  // namespace service_runtime
