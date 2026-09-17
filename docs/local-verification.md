# Local verification record

This repository has been checked on the CPU-only development machine. These results
establish the portable baseline; they do not qualify the DeepStream runtime.

## Passed locally

- The C++17 workspace configures and builds with CMake.
- All five native test executables pass. They cover detection decoding and NMS,
  geometry and OCR, scheduling and voting, event serialization, configuration
  rejection and URI redaction, and metadata copy ownership.
- All five Python tests pass. They cover bundle validation, checksum rejection,
  JSON schemas, and synthetic CTC decoding.
- The private vehicle, plate, and LPRNet ONNX files match the recorded SHA-256
  checksums and declared input/output bindings.
- ONNX Runtime recognizes the retained LPR crop as `89AA15689` with the exact
  expected normalized text.
- Both tracked pipeline profiles pass `traffic-alpr --validate-only`; the RTSP
  profile also renders the intended graph description.

## Not qualified locally

- DeepStream and TensorRT plugin compilation in the pinned container
- TensorRT FP16 or INT8 engine generation and ONNX/engine parity
- Actual GStreamer/DeepStream inference, GPU rectification, and metadata flow
- File and MediaMTX RTSP end-to-end replay
- Kafka publication, degradation, and recovery behavior
- EOS/SIGTERM operation, memory stability, and the 30-minute performance gate

Run the gates in [GPU server handoff](gpu-handoff.md) from the same commit before
calling the application production-ready.
