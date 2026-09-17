# Architecture

The production data path is metadata-driven. DeepStream owns decode, batching,
TensorRT execution, and tracker IDs. The portable ALPR core owns decoding math,
rectification semantics, bounded work selection, CTC, voting, and wire payloads.

```text
source -> decode -> nvstreammux -> vehicle nvinfer -> NvDCF
       -> vehicle ROI/plate nvinfer -> plate keypoint metadata
       -> 156x32 BGR rectification -> LPRNet -> per-track vote
       -> event metadata -> stdout and Kafka
```

DeepStream structs are converted at plugin boundaries into `alpr::TensorView`,
`alpr::ImageView`, and domain metadata. Nothing in `libs/alpr-core` includes a platform
SDK. Plate decoding accepts the original ONNX channel-first heads and the historical
converted channel-last heads, but the tracked model bundle selects the original ONNX
contract.

TensorRT plans are caches, not releases. Their identity includes model hashes, parser
ABI, precision, batch, TensorRT/DeepStream versions, and GPU architecture. The runtime
must reject an engine-set descriptor that does not match the active bundle/environment.

The local build validates contracts and algorithms only. The plugin sources are narrow
ABI adapters; attaching plate keypoints from raw tensor metadata, GPU rectification,
the complete GStreamer application graph, Kafka reconnection, and engine compatibility
remain GPU-server qualification work.

