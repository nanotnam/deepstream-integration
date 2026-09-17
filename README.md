# DeepStream Integration

An app-centered C++17 workspace for NVIDIA DeepStream pipelines. The first application
is Traffic ALPR: vehicle detection, tracking, every-frame plate detection for every
in-zone vehicle, keypoint-based
rectification, LPRNet/CTC recognition, voting, and JSON/Kafka publication.

```text
apps/traffic-alpr/       deployable service, profiles, and model contracts
libs/alpr-core/          platform-neutral ALPR algorithms
libs/deepstream-runtime/ configuration, metadata, and pipeline orchestration
plugins/                 DeepStream ABI adapters, built only with the SDK
tools/model-builder/     ONNX/TensorRT validation and engine tooling
tests/                   native and GPU integration tests
```

## Local verification

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
build/apps/traffic-alpr/traffic-alpr \
  --config apps/traffic-alpr/configs/file.yaml --validate-only
```

The local build deliberately cannot run inference. It validates portable behavior and
fails clearly if asked to execute without a DeepStream-qualified build.

Plate work is lossless at the application boundary: every active tracked vehicle whose
bottom-center lies inside the recognition zone creates one job per processed frame.
Batch size controls how jobs are grouped, not whether they are admitted.

## Private artifacts

Place the three private ONNX inputs under `.local/models/1.0.0/` and generated
TensorRT plans under `.local/engines/`. ONNX files, engines, calibration images,
datasets, credentials, and evidence are ignored. Nothing in the build or runtime
depends on another repository.

See the [local verification record](docs/local-verification.md),
[operations](apps/traffic-alpr/docs/operations.md), and the
[GPU server handoff](docs/gpu-handoff.md) for continuation.
