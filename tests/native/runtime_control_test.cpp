#include "deepstream_runtime/runtime_control.hpp"

#include <cassert>

int main() {
  using deepstream_runtime::ServiceState;
  assert(std::string(deepstream_runtime::service_state_name(ServiceState::kStarting)) ==
         "starting");
  assert(std::string(deepstream_runtime::service_state_name(ServiceState::kDegraded)) ==
         "degraded");

  deepstream_runtime::ReconnectBackoff backoff(1000U, 30000U);
  assert(backoff.next_delay_ms() == 1000U);
  assert(backoff.next_delay_ms() == 2000U);
  assert(backoff.next_delay_ms() == 4000U);
  for (size_t index = 0U; index < 8U; ++index) static_cast<void>(backoff.next_delay_ms());
  assert(backoff.next_delay_ms() == 30000U);
  backoff.reset();
  assert(backoff.next_delay_ms() == 1000U);

  deepstream_runtime::PublishQueue queue(2U);
  assert(queue.push({"events", "one"}));
  assert(queue.push({"events", "two"}));
  assert(!queue.push({"events", "three"}));
  assert(queue.rejected() == 1U);
  assert(queue.depth() == 2U);
  assert(queue.pop()->payload == "one");
  assert(queue.pop()->payload == "two");
  queue.close();
  assert(!queue.pop().has_value());
  assert(!queue.push({"events", "four"}));
  assert(queue.rejected() == 2U);
  return 0;
}
