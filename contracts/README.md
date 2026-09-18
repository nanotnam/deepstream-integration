# Wire contracts

Contracts in this directory are shared by producers and consumers. A published schema
version is immutable: compatible additions require a new schema revision and breaking
changes require a new major topic version.

Every contract directory contains `schema.json` and a validated `example.json`.
The JSON payload carries its schema/version in its required `schema` property. The
Kafka record key follows the table below.

| Topic | Key | Contract | Retention intent |
| --- | --- | --- | --- |
| `mbfs.alpr.events.v1` | `source_id` | `mbfs.alpr.event.v1` | durable event history |
| `mbfs.alpr.health.v1` | `source_id` | `mbfs.alpr.health.v1` | compacted current state |
| `mbfs.alpr.events.retry.v1` | `event_id` | original event | bounded redelivery |
| `mbfs.alpr.events.dlq.v1` | original record key | dead-letter envelope | operator inspection |

`event_id` is the idempotency key for external delivery. Consumers must tolerate the
same event more than once because Kafka-to-HTTP delivery is at-least-once.

The Traffic ALPR process represents a message with an in-memory key and optional
headers. Its direct NVIDIA `nvds_msgapi` publisher currently sends only the topic and
JSON payload; the Kafka adapter derives the record key from the payload's `source_id`
using `partition-key = source_id`. It does not transmit the in-memory message headers.
Consumers must therefore read the schema from the JSON payload and must not require a
Kafka `schema` header. Retry and dead-letter records produced by the event-forwarder
use the regular Kafka client and may carry their own operational headers.
