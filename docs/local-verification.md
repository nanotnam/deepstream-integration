# Local verification record

This repository has been checked in both the portable host build and the pinned
DeepStream 9.1 container on the target RTX 3060.

## Passed locally

- The C++17 workspace configures and builds with CMake.
- All nine native test executables pass. They cover detection decoding and NMS,
  geometry and OCR, every-vehicle zone admission, batch splitting, blocking queue
  behavior, voting, event serialization, configuration rejection and URI redaction,
  and metadata copy ownership.
- All eight Python tests pass. They cover bundle validation, batch profiles, checksum rejection,
  JSON schemas, and synthetic CTC decoding.
- The private vehicle, plate, and LPRNet ONNX files match the recorded SHA-256
  checksums and declared input/output bindings.
- ONNX Runtime recognizes the retained LPR crop as `89AA15689` with the exact
  expected normalized text.
- Both tracked pipeline profiles pass `traffic-alpr --validate-only`; the RTSP
  profile also renders the intended graph description.

## Passed on the target GPU

- Required plugins and the application compile in the digest-pinned DeepStream 9.1
  image.
- The three FP16 TensorRT engines build sequentially with a 2048 MiB workspace and
  pass runtime identity/hash/profile validation.
- File replay exercises decode, NvDCF tracking, GPU preprocessing, plate inference,
  GPU rectification, LPRNet, voting, stdout publication, EOS draining, and SIGTERM.
- An unavailable Kafka endpoint leaves inference running, rejects and counts event
  submissions, reports reconnecting health, and respects the shutdown deadline.
- MediaMTX replay recovers after starting without an available publisher, transitions
  from `reconnecting` to `running`, and produces the same eight unique candidate plate
  strings as file replay across complete loop coverage.

## Remaining qualification

- Compare the emitted plate values with the owner-provided expected-plate manifest.
- Verify publication and recovery against the production Kafka authentication and
  network configuration.
- Run the 30-minute performance/memory gate.
- Complete the automated FP16-versus-ONNX numerical parity and dynamic batch 4/8
  association tests.

Run the gates in [GPU server handoff](gpu-handoff.md) from the same commit before
calling the application production-ready.
