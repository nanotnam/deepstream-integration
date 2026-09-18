# ADR 0002: Kafka delivery semantics

Status: accepted

Inference applications publish versioned events and continue inference when Kafka is
unavailable. The downstream forwarder provides at-least-once delivery. It commits an
offset only after external delivery or successful handoff to a retry/dead-letter
topic. `event_id` is sent as the external idempotency key.

Transient failures use bounded exponential backoff and then enter the retry topic.
Malformed, unsupported, and permanent failures enter the dead-letter topic. Exactly
once delivery to an arbitrary external HTTP service is explicitly not claimed.
