# DeepStream Integration

An application-development workspace for NVIDIA DeepStream pipelines and the services
that integrate their events. The first application is Traffic ALPR: vehicle detection,
tracking, every-frame plate detection for every in-zone vehicle, keypoint-based
rectification, LPRNet/CTC recognition, voting, and JSON/Kafka publication.

```text
apps/traffic-alpr/       complete ALPR app, domain code, plugins, profiles, and tests
libs/deepstream-runtime/ reusable domain-neutral DeepStream platform primitives
libs/messaging/          publication API, bounded queues, and reconnect logic
libs/service-runtime/    lifecycle primitives shared across deployables
contracts/               versioned inter-process JSON contracts
kafka/                   local broker, topic definitions, and operating guidance
services/                independently deployed Kafka consumers/backend adapters
tools/                   build-time and developer utilities
tests/                   cross-component, GPU, and end-to-end tests
```

## Quick start

Run commands from the repository root. The portable build and tests do not require a
GPU, DeepStream, or private models. On Debian/Ubuntu, install the native prerequisites
with:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev \
  python3 python3-venv python3-pip
```

CMake 3.20 or newer and a C++17 compiler are required.

### Portable build and tests

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure

python3 -m venv .venv
.venv/bin/pip install \
  -e 'tools/model-builder[reference,test]' \
  -e 'services/event-forwarder[test]'
.venv/bin/python -m pytest tools/model-builder/tests
.venv/bin/python -m pytest services/event-forwarder/tests

build/apps/traffic-alpr/traffic-alpr \
  --config apps/traffic-alpr/configs/file.yaml --validate-only
```

The local build deliberately cannot run inference. It validates portable behavior and
fails clearly if asked to execute without a DeepStream-qualified build. The Python
tests use generated fixtures and do not require private ONNX files.

### Local Kafka on host port 9093

Host port 9092 belongs to another service on this server. Start this repository's
broker on 9093 and wait for Compose health:

```bash
KAFKA_HOST_PORT=9093 docker compose -f kafka/compose.yaml up -d --wait
KAFKA_HOST_PORT=9093 docker compose -f kafka/compose.yaml ps
docker run --rm --network host \
  --entrypoint /opt/kafka/bin/kafka-topics.sh apache/kafka:4.1.0 \
  --bootstrap-server localhost:9093 --list
```

Host clients use `localhost:9093`. Containers attached to `kafka_default` use
`kafka:29092`; the DeepStream adapter expresses the same endpoint as `kafka;29092`.
See [Kafka operations](kafka/README.md) for details and shutdown instructions.

### Pinned DeepStream image

Building this image compiles the SDK runtime and app-local plugins and runs the native
test suite inside the digest-pinned DeepStream 9.1 environment:

```bash
docker build -f apps/traffic-alpr/Dockerfile -t traffic-alpr:local .
docker run --rm traffic-alpr:local \
  --config /opt/mbfs/share/traffic-alpr/configs/file.yaml --validate-only
```

Image construction does not require a GPU. Engine generation and inference do require
an NVIDIA GPU, NVIDIA Container Toolkit, and the private models under
`.local/models/1.0.0`. Generate compatible engines using the
[model-builder procedure](tools/model-builder/README.md), then place a replay fixture
at `.local/test-video/test.mp4` and run:

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

See [Traffic ALPR operations](apps/traffic-alpr/docs/operations.md) for configuration,
RTSP, queue, and degraded-Kafka behavior. See the
[event-forwarder runbook](services/event-forwarder/README.md) to test the downstream
Kafka-to-HTTP service.

The `services/event-forwarder` consumer validates ALPR events, delivers them to an
external HTTP backend with `event_id` as its idempotency key, and routes bounded
failures to retry or dead-letter topics.

Plate work is lossless at the application boundary: every active tracked vehicle whose
bottom-center lies inside the recognition zone creates one job per processed frame.
Batch size controls how jobs are grouped, not whether they are admitted.

## Private artifacts

Place the three private ONNX inputs under `.local/models/1.0.0/` and generated
TensorRT plans under `.local/engines/`. ONNX files, engines, calibration images,
datasets, credentials, and evidence are ignored. Nothing in the build or runtime
depends on another repository.

See the [local verification record](docs/local-verification.md) and the
[GPU server handoff](docs/gpu-handoff.md) for qualification status.
