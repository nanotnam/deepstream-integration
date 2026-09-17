# Operations

Pass source and broker addresses at runtime; never write credentials into a tracked
profile. `ALPR_SOURCE_URI` accepts a file URI or RTSP URI according to the selected
profile. `ALPR_KAFKA_BROKERS` contains the Kafka connection string expected by the
DeepStream adapter.

`processing.max_fps: 0` processes the full source rate. A nonzero value is an
intentional sampling limit and its skipped frames are reported separately from drops.
Every active vehicle whose bounding-box bottom-center is inside the configured zone is
sent to plate inference on every processed frame. `plate_batch_size` and
`lpr_batch_size` group work; they never cap the number of admitted objects. With the
tracked static models both values must remain `1`.

The plate job queue is bounded and blocking. Queue pressure increases latency instead
of silently losing plate jobs. For live RTSP, sustained overload can therefore produce
growing source latency; monitor queue depth, maximum depth, oldest job age, blocked
pushes, and blocked time.

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
