#pragma once

namespace service_runtime {

enum class ServiceState { kStarting, kRunning, kDegraded, kReconnecting, kStopping };

const char* service_state_name(ServiceState state);

}  // namespace service_runtime
