# Local data

Generated and private material is ignored:

```text
.local/
  import/axis-alpr/
    sources/
    calibration/
    datasets/
    evidence/
  engines/<bundle>/<engine-key>/
  reports/
archive/
```

Do not copy ACAP credentials, `.env` files, EAPs, ARTPEC TFLites, or historical
conversion workspaces. Verify imported assets against a SHA-256 transfer manifest.
Model bundle validation checks the selected ONNX files again before engine generation.

