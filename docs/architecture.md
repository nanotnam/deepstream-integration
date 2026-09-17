# Architecture

The production data path is metadata-driven. DeepStream owns decode, batching,
TensorRT execution, and tracker IDs. The portable ALPR core owns decoding math,
rectification semantics, recognition-zone selection, lossless job queuing, CTC,
voting, and wire payloads.

```text
source -> optional FPS gate -> decode -> nvstreammux[batch=1]
       -> vehicle nvinfer[interval=0] -> NvDCF -> bottom-center zone filter
       -> blocking plate-job queue -> vehicle ROI/plate nvinfer
       -> plate keypoint metadata
       -> 156x32 BGR rectification -> LPRNet -> per-track vote
       -> event metadata -> stdout and Kafka
```

DeepStream structs are converted at plugin boundaries into `alpr::TensorView`,
`alpr::ImageView`, and domain metadata. Nothing in `libs/alpr-core` includes a platform
SDK. Plate decoding accepts the original ONNX channel-first heads and the historical
converted channel-last heads, but the tracked model bundle selects the original ONNX
contract.

Every active in-zone track creates a plate job on every processed frame, including
tracks with a previous successful result. A miss is therefore retried by the next
frame rather than a timer. The queue never uses a leaky overflow policy; capacity
creates upstream backpressure and health signals. Static batch-one models are handled
sequentially today, while the same job mapping and batch decoders support future
dynamic engines.

The DeepStream collector takes a reference on the originating `GstBuffer` for every
queued group of jobs. That lease keeps its `NvBufSurface` and batch metadata alive
until the plate worker finishes, so queueing cannot leave dangling frame pointers.

TensorRT plans are caches, not releases. Their identity includes model hashes, parser
ABI, precision, batch, TensorRT/DeepStream versions, and GPU architecture. The runtime
must reject an engine-set descriptor that does not match the active bundle/environment.

The local build validates contracts, per-frame admission, batching, backpressure,
metadata mapping, and algorithms. The plugin sources are narrow ABI adapters;
attaching plate keypoints from raw tensor metadata, GPU rectification, the complete
GStreamer application graph, Kafka reconnection, and engine compatibility remain
GPU-server qualification work.
