# DeepStream model builder

The tool validates private ONNX files against a tracked bundle and builds serialized
TensorRT plans on the target GPU. Run all commands below from the repository root.

## Portable validation and tests

These steps do not require a GPU or private model files:

```bash
python3 -m venv .venv
.venv/bin/pip install -e 'tools/model-builder[reference,test]'
.venv/bin/python -m pytest tools/model-builder/tests
```

With the private ONNX files under `.local/models/1.0.0`, validate their hashes and
bindings or run the retained CPU reference fixture:

```bash
ALPR_MODEL_ROOT=.local/models/1.0.0 \
  .venv/bin/ds-model validate \
  --bundle apps/traffic-alpr/models/bundles/1.0.0/bundle.yaml

.venv/bin/ds-model reference --role lprnet \
  --image /path/to/rectified-plate.png \
  --expected 89AA15689 --asset-root .local/models/1.0.0 \
  --bundle apps/traffic-alpr/models/bundles/1.0.0/bundle.yaml
```

## Target-specific engine generation

TensorRT engines are caches for one hardware/software target, not portable release
artifacts. Build them on the target GPU architecture and inside the same pinned
DeepStream image used to compile and run Traffic ALPR. Do not use an arbitrary host
`trtexec` installation.

Build the image first, then generate the engines inside it. The temporary virtual
environment installs only the build-time Python tool and never becomes an application
runtime dependency:

```bash
docker build -f apps/traffic-alpr/Dockerfile -t traffic-alpr:local .
mkdir -p .local/engines

docker run --rm --gpus all \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -e CUDA_VERSION=13.2 \
  -e DEEPSTREAM_VERSION=9.1 \
  -e TENSORRT_VERSION=10.16.1 \
  -v "$PWD/.local/models/1.0.0:/models:ro" \
  -v "$PWD/.local/engines:/engines" \
  --entrypoint bash traffic-alpr:local -lc '
    python3 -m venv /tmp/model-builder-venv
    cp -R /workspace/tools/model-builder /tmp/model-builder-src
    /tmp/model-builder-venv/bin/pip install --no-cache-dir /tmp/model-builder-src
    /tmp/model-builder-venv/bin/ds-model build \
      --precision fp16 --workspace-mib 2048 \
      --asset-root /models \
      --output /engines \
      --trtexec /usr/bin/trtexec \
      --bundle /workspace/apps/traffic-alpr/models/bundles/1.0.0/bundle.yaml
  '
```

The version values above describe the pinned DeepStream 9.1 image currently qualified
on the RTX 3060. If the pinned image changes, obtain the actual versions from that
image and update the values together; never label an engine with a version it was not
built against.

The engine key and `engine-set.json` identity include the bundle and source hashes,
parser ABI, precision, batch profiles, full `trtexec --version` result, parsed or
explicit TensorRT version, DeepStream version, CUDA version, GPU name, and GPU compute
capability. Engines are built sequentially with the requested workspace limit. Dynamic
inputs receive explicit TensorRT minimum/optimal/maximum shapes; static inputs must use
a fixed matching batch profile. INT8 builds additionally require pre-generated,
verified TensorRT calibration caches.

Inspect the generated identity record with:

```bash
find .local/engines/1.0.0 -mindepth 2 -maxdepth 2 \
  -name engine-set.json -print -exec python3 -m json.tool {} \;
```

The definitive compatibility check is application startup in the same image. The
runtime hashes every engine, compares each recorded source hash with the tracked model
contract, and rejects a set whose parser ABI, batch profile, SDK versions, GPU
identity, or compute capability does not match. Numerical TensorRT comparison remains
a GPU qualification gate; the local tool does not claim that CPU validation qualifies
an engine.

Private ONNX files, engines, calibration material, datasets, and generated reports
remain under ignored `.local/` paths and must not be committed.
