from __future__ import annotations

import json
import shutil
import subprocess
from dataclasses import asdict
from hashlib import sha256
from pathlib import Path

from .contracts import Bundle, ContractError


def _command_version(executable: str) -> str:
    completed = subprocess.run(
        [executable, "--version"], check=False, capture_output=True, text=True
    )
    text = (completed.stdout + completed.stderr).strip()
    return text.splitlines()[0] if text else "unknown"


def _gpu_name() -> str:
    executable = shutil.which("nvidia-smi")
    if executable is None:
        return "unknown-gpu"
    completed = subprocess.run(
        [executable, "--query-gpu=name", "--format=csv,noheader"],
        check=False,
        capture_output=True,
        text=True,
    )
    return completed.stdout.strip().splitlines()[0] if completed.returncode == 0 else "unknown-gpu"


def engine_key(bundle: Bundle, precision: str, trtexec_version: str, gpu: str) -> str:
    material = json.dumps(
        {
            "bundle": bundle.version,
            "parser_abi": bundle.parser_abi,
            "models": [asdict(model) | {"source": model.source.name} for model in bundle.models],
            "precision": precision,
            "batch": 1,
            "trtexec": trtexec_version,
            "gpu": gpu,
        },
        sort_keys=True,
        default=list,
    )
    return sha256(material.encode("utf-8")).hexdigest()[:16]


def build_engines(
    bundle: Bundle,
    precision: str,
    output_root: Path,
    workspace_mib: int,
    calibration_cache_dir: Path | None,
    trtexec: str = "trtexec",
) -> Path:
    executable = shutil.which(trtexec)
    if executable is None:
        raise ContractError(f"TensorRT executable not found: {trtexec}")
    if precision not in {"fp16", "int8"}:
        raise ContractError("precision must be fp16 or int8")
    if workspace_mib < 256:
        raise ContractError("workspace must be at least 256 MiB")
    if precision == "int8" and calibration_cache_dir is None:
        raise ContractError("INT8 builds require --calibration-cache-dir")

    version = _command_version(executable)
    gpu = _gpu_name()
    destination = output_root / bundle.version / engine_key(bundle, precision, version, gpu)
    destination.mkdir(parents=True, exist_ok=True)
    report: dict[str, object] = {
        "schema": "mbfs.tensorrt-engine-set/v1",
        "bundle": bundle.version,
        "precision": precision,
        "batch": 1,
        "trtexec": version,
        "gpu": gpu,
        "workspace_mib": workspace_mib,
        "engines": {},
    }
    for model in bundle.models:
        engine = destination / f"{model.role}.engine"
        command = [
            executable,
            f"--onnx={model.source}",
            f"--saveEngine={engine}",
            f"--memPoolSize=workspace:{workspace_mib}MiB",
            "--builderOptimizationLevel=3",
            "--skipInference",
            f"--{precision}",
        ]
        if precision == "int8":
            cache = calibration_cache_dir / f"{model.role}.cache"
            if not cache.is_file():
                raise ContractError(f"INT8 calibration cache is missing: {cache}")
            command.append(f"--calib={cache}")
        completed = subprocess.run(command, check=False, text=True)
        if completed.returncode != 0:
            raise ContractError(f"TensorRT build failed for {model.role}")
        report["engines"][model.role] = {
            "file": engine.name,
            "sha256": _file_sha256(engine),
            "source_sha256": model.sha256,
        }
    (destination / "engine-set.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return destination


def _file_sha256(path: Path) -> str:
    digest = sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

