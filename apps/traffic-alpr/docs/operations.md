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
docker build -f apps/traffic-alpr/Dockerfile -t traffic-alpr:local .
docker run --rm traffic-alpr:local \
  --config /opt/mbfs/share/traffic-alpr/configs/file.yaml --validate-only
```

Run these commands from the repository root. The Dockerfile pins the DeepStream 9.1
base image by digest and builds the SDK-dependent runtime and plugins. The first path
in each `-v` argument is a host path; the second is its container path. The production
container installs profiles under `/opt/mbfs/share/traffic-alpr/configs`.

Start the repository Kafka stack before replay (see `kafka/README.md`). Generated
engines must exist under host path `.local/engines`; source media must exist under
`.local/test-video`. The tracked file profile resolves its relative engine root from
the container working directory, so `.local/engines` maps to
`/opt/mbfs/.local/engines`. A file replay is:

```bash
docker run --rm --gpus all \
  --network kafka_default \
  -e ALPR_SOURCE_URI=file:///data/test.mp4 \
  -e 'ALPR_KAFKA_BROKERS=kafka;29092' \
  -v "$PWD/.local/engines:/opt/mbfs/.local/engines:ro" \
  -v "$PWD/.local/test-video:/data:ro" \
  traffic-alpr:local \
  --config /opt/mbfs/share/traffic-alpr/configs/file.yaml
```

Do not pass broker credentials on a shared command line. Use the deployment
platform's secret environment injection when authentication is required.

Print the intended graph:

```bash
docker run --rm traffic-alpr:local \
  --config /opt/mbfs/share/traffic-alpr/configs/rtsp.yaml --print-gst-graph
```

The printed publishing stage is descriptive application architecture, not an extra
GStreamer branch: completed JSON is handed to the application's direct
`nvds_msgapi` Kafka publisher.

The service emits `mbfs.alpr.event.v1` JSON to `mbfs.alpr.events.v1` and
`mbfs.alpr.health.v1` JSON to `mbfs.alpr.health.v1`. Kafka failure is degraded service,
not permission to stop inference. While reconnecting, submissions are rejected and
counted instead of being retained as a durable local spool. Set
`outputs.kafka.enabled: false` in a private replay profile when a broker is not yet
available; stdout output remains independent.
