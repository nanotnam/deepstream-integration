from __future__ import annotations

from pathlib import Path

from ds_model.contracts import Bundle, ModelContract
from ds_model.engine import _tensorrt_version, engine_key


def bundle() -> Bundle:
    model = ModelContract(
        role="vehicle",
        source=Path("vehicle.onnx"),
        sha256="a" * 64,
        input_name="input",
        input_shape=(1, 3, 8, 8),
        dynamic_batch=False,
        batch_minimum=1,
        batch_optimal=1,
        batch_maximum=1,
        input_dtype="float32",
        color="RGB",
        resize="stretch",
        output_names=("output",),
        mean=(0.0, 0.0, 0.0),
        std=(1.0, 1.0, 1.0),
        pad_value=0,
    )
    return Bundle(version="1.0.0", parser_abi=1, models=(model,))


def test_engine_key_includes_target_runtime() -> None:
    first = engine_key(bundle(), "fp16", "10.0", "RTX 3060", "8.6", "9.1", "13.0")
    assert first == engine_key(
        bundle(), "fp16", "10.0", "RTX 3060", "8.6", "9.1", "13.0"
    )
    assert first != engine_key(
        bundle(), "fp16", "10.0", "RTX 3060", "8.9", "9.1", "13.0"
    )
    assert first != engine_key(
        bundle(), "fp16", "10.0", "RTX 3060", "8.6", "9.0", "13.0"
    )


def test_extracts_tensorrt_version(monkeypatch) -> None:
    monkeypatch.delenv("TENSORRT_VERSION", raising=False)
    assert _tensorrt_version("TensorRT.trtexec [TensorRT v10.8.0]") == "10.8.0"
    assert _tensorrt_version("TensorRT.trtexec [TensorRT v101601]") == "10.16.1"
    monkeypatch.setenv("TENSORRT_VERSION", "10.9.1")
    assert _tensorrt_version("unparseable") == "10.9.1"
