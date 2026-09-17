# DeepStream model builder

The tool validates private ONNX files against a tracked bundle and builds serialized
TensorRT plans on the target GPU. It never converts an ARTPEC TFLite.

```bash
python3 -m venv .venv
.venv/bin/pip install -e '.[reference,test]'

ALPR_MODEL_ROOT=.local/import/axis-alpr/sources \
  .venv/bin/ds-model validate \
  --bundle ../../apps/traffic-alpr/models/bundles/1.0.0/bundle.yaml

.venv/bin/ds-model reference --role lprnet \
  --image .local/import/axis-alpr/calibration/lprnet/track_41_89AA_15689_raw.png \
  --expected 89AA15689 --asset-root .local/import/axis-alpr/sources \
  --bundle ../../apps/traffic-alpr/models/bundles/1.0.0/bundle.yaml

.venv/bin/ds-model build --precision fp16 --workspace-mib 2048 \
  --asset-root .local/import/axis-alpr/sources \
  --bundle ../../apps/traffic-alpr/models/bundles/1.0.0/bundle.yaml
```

Engines are built sequentially and recorded with their source hashes, TensorRT version,
GPU name, precision, batch, and parser ABI. INT8 builds require pre-generated, verified
TensorRT calibration caches. Numerical TensorRT comparison remains a GPU-server gate;
the local tool reports that limitation explicitly rather than pretending CPU validation
qualifies a TensorRT plan.
