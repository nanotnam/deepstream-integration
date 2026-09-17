# DeepStream integration tests

These tests run only on the GPU server. They require the pinned DeepStream image, the
ignored model/dataset mounts, a local Kafka-compatible broker, and MediaMTX. The suite
must cover plugin loading, engine compatibility, file/RTSP equivalence, broker outage,
RTSP reconnect, EOS, SIGTERM, numerical parity, and the 30-minute performance gate.

No marker in this directory is evidence that GPU qualification passed. Store reports
under `.local/reports/` and record only reviewed hashes and summaries in a release.
