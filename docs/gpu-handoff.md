# GPU server handoff

Keep source and build output on the Linux filesystem at
`/home/ai-team2/deepstream-integration`. Mount large private assets read-only from
`/mnt/data_ntfs/namvh/deepstream-integration/.local/` where practical.

Before GPU work, record OS, driver, GPU, Docker, NVIDIA Container Toolkit, DeepStream,
CUDA, and TensorRT versions. Stop competing GPU workloads and confirm at least 8 GB is
free before building engines. Build vehicle, plate, and LPRNet plans sequentially with
a 2048 MiB workspace limit and retain `engine-set.json`.

Required qualification gates:

1. Compile all plugin targets in the pinned DeepStream 9.1 image.
2. Compare FP16 decoded results with the ONNX CPU reference.
3. Complete keypoint metadata attachment and GPU rectification wiring.
4. Run file replay, then the same fixture through MediaMTX RTSP.
5. Verify Kafka events and five-second health records during broker recovery.
6. Sustain one 1080p30 source for 30 minutes at 29.5 FPS or better with under one
   percent application drops and stable memory.
7. Build INT8 only after FP16 passes, then enforce the retained parity gate.

