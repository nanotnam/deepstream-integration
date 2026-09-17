# DeepStream model builder

The tool validates private ONNX files against a tracked bundle and builds serialized
TensorRT plans on the target GPU.

```bash
python3 -m venv .venv
.venv/bin/pip install -e 'tools/model-builder[reference,test]'

ALPR_MODEL_ROOT=.local/models/1.0.0 \
  .venv/bin/ds-model validate \
  --bundle apps/traffic-alpr/models/bundles/1.0.0/bundle.yaml

.venv/bin/ds-model reference --role lprnet \
  --image /path/to/rectified-plate.png \
  --expected 89AA15689 --asset-root .local/models/1.0.0 \
  --bundle apps/traffic-alpr/models/bundles/1.0.0/bundle.yaml

.venv/bin/ds-model build --precision fp16 --workspace-mib 2048 \
  --asset-root .local/models/1.0.0 \
  --bundle apps/traffic-alpr/models/bundles/1.0.0/bundle.yaml
```

Engines are built sequentially and recorded with their source hashes, TensorRT version,
GPU name, precision, batch profile, and parser ABI. Dynamic inputs receive explicit
TensorRT minimum/optimal/maximum shapes; static inputs are required to use a fixed
matching batch profile. INT8 builds require pre-generated, verified
TensorRT calibration caches. Numerical TensorRT comparison remains a GPU-server gate;
the local tool reports that limitation explicitly rather than pretending CPU validation
qualifies a TensorRT plan.
