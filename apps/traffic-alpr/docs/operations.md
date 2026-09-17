# Operations

Pass source and broker addresses at runtime; never write credentials into a tracked
profile. `ALPR_SOURCE_URI` accepts a file URI or RTSP URI according to the selected
profile. `ALPR_KAFKA_BROKERS` contains the Kafka connection string expected by the
DeepStream adapter.

Validate configuration without initializing DeepStream:

```bash
traffic-alpr --config configs/file.yaml --validate-only
```

Print the intended graph:

```bash
traffic-alpr --config configs/rtsp.yaml --print-gst-graph
```

The service emits `mbfs.alpr.event.v1` JSON to `mbfs.alpr.events.v1` and
`mbfs.alpr.health.v1` JSON to `mbfs.alpr.health.v1`. Kafka failure is degraded service,
not permission to stop inference. Version 1 does not provide a durable local spool.

