# GPU server handoff

Run commands from the repository root and use repository-relative paths. The
2026-09-18 qualification was performed in
`/mnt/data_ntfs/namvh/deepstream-integration`; that location records provenance, not a
required checkout path. Keep private models, generated engines, fixtures, and evidence
under the checkout's ignored `.local/` directory and mount them read-only where the
operation permits.

Before GPU work, record OS, driver, GPU, Docker, NVIDIA Container Toolkit, DeepStream,
CUDA, and TensorRT versions. Stop competing GPU workloads and confirm at least 8 GB is
free before building engines. Build vehicle, plate, and LPRNet plans sequentially with
a 2048 MiB workspace limit and retain `engine-set.json`.

Both `nvidia-smi` on the host and `docker run --rm --gpus all ... nvidia-smi` must
pass before building. The runtime rejects an engine set unless its engine hashes,
source hashes, parser ABI, batch profiles, SDK versions, GPU identity, and compute
capability match the active process.

Required qualification gates:

1. [complete] Compile all plugin targets in the pinned DeepStream 9.1 image.
2. Compare FP16 decoded results with the ONNX CPU reference.
3. [complete] Bind the `PlateWorker` handler to GPU ROI preprocessing and plate
   inference, preserving `(frame, track, batch-index)` association.
4. [complete] Run GPU rectification and LPR inference for every admitted job; file
   replay confirms continued inference after stable publication.
5. [baseline only; rerun pending] The pre-refactor MediaMTX fixture matched file replay,
   but the renamed runtime/plugins have not repeated this gate.
6. [partial] Local unauthenticated delivery and unavailable-endpoint degradation are
   verified. Successful reconnect with the production Kafka authentication and network
   configuration remains an environment-specific gate.
7. Sustain one 1080p30 source for 30 minutes at 29.5 FPS or better with under one
   percent application drops and stable memory.
8. Build INT8 only after FP16 passes, then enforce the retained parity gate.
9. [complete] Confirm the DeepStream Kafka protocol adapter preserves the logical
   `source_id` message key carried by `messaging::Message`. On 2026-09-18 a real
   consumer observed `qualification-source` for the focused adapter probe and
   `replay-01` for all application event and health records.
