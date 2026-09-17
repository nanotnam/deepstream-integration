from __future__ import annotations

from dataclasses import dataclass
from hashlib import sha256
from pathlib import Path
from typing import Any

import yaml


class ContractError(ValueError):
    """A model bundle is malformed or does not match its assets."""


@dataclass(frozen=True)
class ModelContract:
    role: str
    source: Path
    sha256: str
    input_name: str
    input_shape: tuple[int, ...]
    input_dtype: str
    color: str
    resize: str
    output_names: tuple[str, ...]
    mean: tuple[float, float, float]
    std: tuple[float, float, float]
    pad_value: int


@dataclass(frozen=True)
class Bundle:
    version: str
    parser_abi: int
    models: tuple[ModelContract, ...]


def file_sha256(path: Path) -> str:
    digest = sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_bundle(path: Path, asset_root: Path, verify_files: bool = True) -> Bundle:
    try:
        document: Any = yaml.safe_load(path.read_text(encoding="utf-8"))
    except (OSError, yaml.YAMLError) as exc:
        raise ContractError(f"cannot read bundle {path}: {exc}") from exc
    if not isinstance(document, dict) or document.get("schema") != "mbfs.deepstream-model-bundle/v1":
        raise ContractError("schema must be mbfs.deepstream-model-bundle/v1")
    version = document.get("bundle")
    parser_abi = document.get("parser_abi")
    models = document.get("models")
    if not isinstance(version, str) or not version:
        raise ContractError("bundle must be a non-empty string")
    if not isinstance(parser_abi, int) or parser_abi < 1:
        raise ContractError("parser_abi must be a positive integer")
    if not isinstance(models, dict) or set(models) != {"vehicle", "plate", "lprnet"}:
        raise ContractError("models must contain exactly vehicle, plate, and lprnet")

    contracts: list[ModelContract] = []
    for role in ("vehicle", "plate", "lprnet"):
        model = models[role]
        if not isinstance(model, dict):
            raise ContractError(f"models.{role} must be a mapping")
        required = (
            "source", "sha256", "input_name", "input_shape", "input_dtype",
            "color", "resize", "output_names", "mean", "std", "pad_value",
        )
        missing = [key for key in required if key not in model]
        if missing:
            raise ContractError(f"models.{role} is missing {', '.join(missing)}")
        shape = model["input_shape"]
        if not isinstance(shape, list) or not shape or any(not isinstance(x, int) or x <= 0 for x in shape):
            raise ContractError(f"models.{role}.input_shape must contain positive integers")
        digest = model["sha256"]
        if not isinstance(digest, str) or len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
            raise ContractError(f"models.{role}.sha256 must be lowercase SHA-256")
        root = asset_root.resolve()
        source = (root / str(model["source"])).resolve()
        if not source.is_relative_to(root):
            raise ContractError(f"models.{role}.source escapes the asset root")
        output_names = model["output_names"]
        if (
            not isinstance(output_names, list)
            or not output_names
            or any(not isinstance(name, str) or not name for name in output_names)
            or len(set(output_names)) != len(output_names)
        ):
            raise ContractError(f"models.{role}.output_names must be unique non-empty strings")
        mean = model["mean"]
        std = model["std"]
        if (
            not isinstance(mean, list)
            or not isinstance(std, list)
            or len(mean) != 3
            or len(std) != 3
            or any(not isinstance(x, (int, float)) for x in mean + std)
            or any(float(x) <= 0 for x in std)
        ):
            raise ContractError(f"models.{role} mean/std must be three numeric values with positive std")
        pad_value = model["pad_value"]
        if not isinstance(pad_value, int) or not 0 <= pad_value <= 255:
            raise ContractError(f"models.{role}.pad_value must be in [0,255]")
        if verify_files:
            if not source.is_file():
                raise ContractError(f"models.{role} source is missing: {source}")
            actual = file_sha256(source)
            if actual != digest:
                raise ContractError(f"models.{role} SHA-256 mismatch: expected {digest}, got {actual}")
        contracts.append(
            ModelContract(
                role=role,
                source=source,
                sha256=digest,
                input_name=str(model["input_name"]),
                input_shape=tuple(shape),
                input_dtype=str(model["input_dtype"]),
                color=str(model["color"]),
                resize=str(model["resize"]),
                output_names=tuple(output_names),
                mean=tuple(float(x) for x in mean),
                std=tuple(float(x) for x in std),
                pad_value=pad_value,
            )
        )
    return Bundle(version=version, parser_abi=parser_abi, models=tuple(contracts))


def validate_onnx_bindings(bundle: Bundle) -> list[dict[str, object]]:
    try:
        import onnxruntime as ort
    except ImportError as exc:
        raise ContractError(
            "ONNX Runtime is unavailable; install the model-builder 'reference' extra"
        ) from exc
    reports: list[dict[str, object]] = []
    for model in bundle.models:
        session = ort.InferenceSession(str(model.source), providers=["CPUExecutionProvider"])
        inputs = session.get_inputs()
        matching = next((item for item in inputs if item.name == model.input_name), None)
        if matching is None:
            raise ContractError(f"{model.role}: input {model.input_name!r} was not found")
        actual_shape = tuple(item if isinstance(item, int) else -1 for item in matching.shape)
        if len(actual_shape) != len(model.input_shape):
            raise ContractError(
                f"{model.role}: input rank {len(actual_shape)} does not match {len(model.input_shape)}"
            )
        for expected, actual in zip(model.input_shape, actual_shape, strict=True):
            if actual != -1 and actual != expected:
                raise ContractError(
                    f"{model.role}: input shape {actual_shape} does not match {model.input_shape}"
                )
        output_names = tuple(item.name for item in session.get_outputs())
        if output_names != model.output_names:
            raise ContractError(
                f"{model.role}: outputs {output_names} do not match {model.output_names}"
            )
        expected_type = {"float32": "tensor(float)"}.get(model.input_dtype)
        if expected_type is None or matching.type != expected_type:
            raise ContractError(
                f"{model.role}: input type {matching.type} does not match {model.input_dtype}"
            )
        reports.append(
            {
                "role": model.role,
                "input": matching.name,
                "shape": list(actual_shape),
                "outputs": list(output_names),
            }
        )
    return reports
