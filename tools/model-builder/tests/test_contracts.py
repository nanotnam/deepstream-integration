from __future__ import annotations

from hashlib import sha256
from pathlib import Path

import pytest
import yaml

from ds_model.contracts import ContractError, load_bundle


def write_bundle(tmp_path: Path, corrupt: bool = False) -> Path:
    models: dict[str, dict[str, object]] = {}
    for role in ("vehicle", "plate", "lprnet"):
        source = tmp_path / f"{role}.onnx"
        source.write_bytes(role.encode())
        models[role] = {
            "source": source.name,
            "sha256": "0" * 64 if corrupt else sha256(role.encode()).hexdigest(),
            "input_name": "input",
            "input_shape": [1, 3, 8, 8],
            "input_dtype": "float32",
            "color": "RGB",
            "resize": "stretch",
            "output_names": ["output"],
            "mean": [0.0, 0.0, 0.0],
            "std": [1.0, 1.0, 1.0],
            "pad_value": 0,
        }
    path = tmp_path / "bundle.yaml"
    path.write_text(
        yaml.safe_dump(
            {
                "schema": "mbfs.deepstream-model-bundle/v1",
                "bundle": "test",
                "parser_abi": 1,
                "models": models,
            }
        ),
        encoding="utf-8",
    )
    return path


def test_loads_and_verifies_bundle(tmp_path: Path) -> None:
    bundle = load_bundle(write_bundle(tmp_path), tmp_path)
    assert bundle.version == "test"
    assert [model.role for model in bundle.models] == ["vehicle", "plate", "lprnet"]


def test_rejects_checksum_mismatch(tmp_path: Path) -> None:
    with pytest.raises(ContractError, match="SHA-256 mismatch"):
        load_bundle(write_bundle(tmp_path, corrupt=True), tmp_path)


def test_rejects_missing_role(tmp_path: Path) -> None:
    path = write_bundle(tmp_path)
    document = yaml.safe_load(path.read_text(encoding="utf-8"))
    del document["models"]["plate"]
    path.write_text(yaml.safe_dump(document), encoding="utf-8")
    with pytest.raises(ContractError, match="exactly"):
        load_bundle(path, tmp_path)
