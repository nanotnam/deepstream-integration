# Local data

Generated and private material is ignored:

```text
.local/
  models/<bundle>/
    vehicle.onnx
    plate.onnx
    lprnet.onnx
  calibration/
  datasets/
  evidence/
  engines/<bundle>/<engine-key>/
  reports/
archive/
```

Only the three source ONNX models are required to build FP16 engines. Calibration data
is needed later for INT8. Do not copy credentials, `.env` files, or historical
conversion workspaces. Model bundle validation checks the selected ONNX files against
their tracked SHA-256 hashes before engine generation.
