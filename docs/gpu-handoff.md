# GPU server handoff

Keep source and build output on the Linux filesystem at
`/home/ai-team2/deepstream-integration`. Mount large private assets read-only from
`/mnt/data_ntfs/namvh/deepstream-integration/.local/` where practical.

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
5. [complete] Run the same fixture through MediaMTX RTSP and compare candidate results.
6. [outage complete] Verify successful delivery and recovery using the production
   Kafka authentication and network configuration.
7. Sustain one 1080p30 source for 30 minutes at 29.5 FPS or better with under one
   percent application drops and stable memory.
8. Build INT8 only after FP16 passes, then enforce the retained parity gate.
