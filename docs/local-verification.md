# Local verification record

The workspace refactor and its SDK-dependent targets were checked on the host and on
the target GPU on 2026-09-18.

## Passed locally

- The C++17 workspace configures and builds with CMake.
- All ten native test executables pass. They cover shared messaging behavior,
  detection decoding and NMS,
  geometry and OCR, every-vehicle zone admission, batch splitting, blocking queue
  behavior, voting, event serialization, configuration rejection and URI redaction,
  and metadata copy ownership.
- All eight model-tool Python tests pass. They cover bundle validation, batch profiles,
  checksum rejection, JSON schemas, and synthetic CTC decoding.
- All five event-forwarder tests pass. They cover successful delivery, contract
  rejection, bounded retry handoff, safe behavior when DLQ publication fails, and
  health metrics.
- The private vehicle, plate, and LPRNet ONNX files match the recorded SHA-256
  checksums and declared input/output bindings.
- ONNX Runtime recognizes the retained LPR crop as `89AA15689` with the exact
  expected normalized text.
- Both tracked pipeline profiles pass `traffic-alpr --validate-only`; the RTSP
  profile also renders the intended graph description.

## Passed on the target GPU after the workspace refactor

- The renamed runtime, application, and all three app-local plugins compile in the
  digest-pinned DeepStream 9.1 image; all ten native tests pass in its Release build.
- The vehicle and plate parser exports are present in the installed inference plugin.
- The three FP16 TensorRT engines build sequentially on the RTX 3060 with a 2048 MiB
  workspace. The retained engine manifest identifies CUDA 13.2, TensorRT 10.16.1,
  DeepStream 9.1, compute capability 8.6, and passes runtime identity/hash/profile
  validation.
- File replay exercises decode, NvDCF tracking, GPU preprocessing, plate inference,
  GPU rectification, LPRNet, voting, stdout publication, Kafka publication, and EOS
  draining. It processed 387/387 frames with no application drops or inference
  failures, emitted eight events, drained 2,891 plate/LPR jobs, and exited cleanly.
- A real consumer on the isolated Kafka development broker observed `replay-01` as
  the record key for every replay event and health record. A focused adapter probe
  likewise observed `qualification-source` as the key. The adapter configuration
  selects the JSON `source_id` field as its `partition-key`.
- The workspace broker was qualified on host port 9093 while the unrelated broker on
  9092 remained running; containers use `kafka:29092`.
- An unavailable Kafka endpoint leaves inference running, rejects and counts event
  submissions, reports reconnecting health, and respects the shutdown deadline.

## Retained pre-refactor GPU baseline

- Before the workspace refactor, MediaMTX replay recovered after starting without an
  available publisher, transitioned from `reconnecting` to `running`, and produced the
  same eight unique candidate plate strings as file replay across complete loop
  coverage. This result was not rerun after the refactor and is not fresh evidence for
  the renamed runtime/plugins.

## Remaining qualification

- Compare the emitted plate values with the owner-provided expected-plate manifest.
- Rerun the MediaMTX RTSP recovery/equivalence gate with the renamed runtime and
  app-local plugins.
- Verify publication and recovery against the production Kafka authentication and
  network configuration.
- Run the 30-minute performance/memory gate.
- Complete the automated FP16-versus-ONNX numerical parity and dynamic batch 4/8
  association tests.

Run the gates in [GPU server handoff](gpu-handoff.md) from the same commit before
calling the application production-ready.
