#include "messaging/runtime.hpp"

#include <cassert>
#include <stdexcept>

int main() {
  messaging::ReconnectBackoff backoff(100U, 400U);
  assert(backoff.next_delay_ms() == 100U);
  assert(backoff.next_delay_ms() == 200U);
  assert(backoff.next_delay_ms() == 400U);
  assert(backoff.next_delay_ms() == 400U);
  backoff.reset();
  assert(backoff.next_delay_ms() == 100U);

  messaging::PublishQueue queue(1U);
  assert(queue.push({"events", "payload", "source-01", {}}));
  assert(!queue.push({"events", "rejected", "source-01", {}}));
  assert(queue.rejected() == 1U);
  const auto message = queue.pop();
  assert(message.has_value());
  assert(message->key == "source-01");
  queue.close();
  assert(!queue.pop().has_value());
  return 0;
}
