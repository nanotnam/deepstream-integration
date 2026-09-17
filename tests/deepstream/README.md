# DeepStream integration tests

These tests run only on the GPU server. They require the pinned DeepStream image, the
ignored model/dataset mounts, a local Kafka-compatible broker, and MediaMTX. The suite
must cover plugin loading, engine compatibility, file/RTSP equivalence, broker outage,
RTSP reconnect, EOS, SIGTERM, numerical parity, and the 30-minute performance gate.

Every-vehicle coverage cases are mandatory: frames with 0, 1, 3, and 11 in-zone
vehicles; batch sizes 1, 4, and 8; a miss followed by inference on the next frame;
continued inference after OCR finalization; bottom-center zone boundaries; and queue
saturation with zero silent job loss. Benchmark-configured FPS skips must be counted
separately from application drops.

No marker in this directory is evidence that GPU qualification passed. Store reports
under `.local/reports/` and record only reviewed hashes and summaries in a release.
