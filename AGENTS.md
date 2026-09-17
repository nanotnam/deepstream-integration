# Repository Guidelines

## Boundaries

`apps/traffic-alpr/` owns the deployable service, profiles, model bundle descriptors,
and operating documentation. `libs/alpr-core/` is C++17 and platform-neutral: it must
not include CUDA, TensorRT, GStreamer, DeepStream, or Axis headers.
`libs/deepstream-runtime/` owns configuration and runtime orchestration. DeepStream ABI
adapters live under `plugins/`. The Python environment under `tools/model-builder/` is
isolated and must never become a runtime dependency.

`archive/` is ignored reference material, never an include path or build input. Private
models, engines, calibration images, datasets, credentials, and generated evidence live
under `.local/` and are never committed.

## Commands

- Native: `cmake -S . -B build && cmake --build build --parallel && ctest --test-dir build --output-on-failure`
- Model tool: `python3 -m pytest tools/model-builder/tests`
- Static checks: `git diff --check`
- DeepStream adapters: configure with `-DDEEPSTREAM_SDK_ROOT=/opt/nvidia/deepstream/deepstream` inside the pinned container.

All C++ targets use `-Wall -Wextra -Wpedantic -Werror`. Tests must cover rejection and
determinism as well as successful paths. Do not add a production CPU inference fallback;
the CPU reference runner exists only for contract and numerical comparison.

## Releases and security

Never log RTSP URLs containing credentials, Kafka secrets, or local private paths. A
model bundle is identified by source hashes, preprocessing, parser ABI, precision, SDK,
TensorRT, GPU compute capability, and batch size. TensorRT engines are target-specific
generated files, not source artifacts.

