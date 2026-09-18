from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
from dataclasses import asdict
from hashlib import sha256
from pathlib import Path

from .contracts import Bundle, ContractError, ModelContract


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


def _gpu_compute_capability() -> str:
    executable = shutil.which("nvidia-smi")
    if executable is None:
        return "unknown"
    completed = subprocess.run(
        [executable, "--query-gpu=compute_cap", "--format=csv,noheader"],
        check=False,
        capture_output=True,
        text=True,
    )
    return completed.stdout.strip().splitlines()[0] if completed.returncode == 0 else "unknown"


def _runtime_version(environment_name: str, fallback: str = "unknown") -> str:
    value = os.environ.get(environment_name, "").strip()
    return value or fallback


def _tensorrt_version(trtexec_version: str) -> str:
    configured = _runtime_version("TENSORRT_VERSION", "")
    if configured:
        return configured
    match = re.search(r"TensorRT[^0-9]*([0-9]+(?:\.[0-9]+){1,3})", trtexec_version)
    if match:
        return match.group(1)
    compact = re.search(r"TensorRT\s+v([0-9]+)", trtexec_version)
    if compact:
        encoded = int(compact.group(1))
        return f"{encoded // 10000}.{(encoded // 100) % 100}.{encoded % 100}"
    return trtexec_version


def engine_key(
    bundle: Bundle,
    precision: str,
    trtexec_version: str,
    gpu: str,
    compute_capability: str = "unknown",
    deepstream_version: str = "unknown",
    cuda_version: str = "unknown",
) -> str:
    material = json.dumps(
        {
            "bundle": bundle.version,
            "parser_abi": bundle.parser_abi,
            "models": [asdict(model) | {"source": model.source.name} for model in bundle.models],
            "precision": precision,
            "batch": 1,
            "trtexec": trtexec_version,
            "gpu": gpu,
            "gpu_compute_capability": compute_capability,
            "deepstream": deepstream_version,
            "cuda": cuda_version,
        },
        sort_keys=True,
        default=list,
    )
    return sha256(material.encode("utf-8")).hexdigest()[:16]


def _profile_shape(model: ModelContract, batch_size: int) -> str:
    shape = (batch_size, *model.input_shape[1:])
    return f"{model.input_name}:" + "x".join(str(dimension) for dimension in shape)


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
    compute_capability = _gpu_compute_capability()
    deepstream_version = _runtime_version("DEEPSTREAM_VERSION", "9.1")
    cuda_version = _runtime_version("CUDA_VERSION")
    tensorrt_version = _tensorrt_version(version)
    destination = output_root / bundle.version / engine_key(
        bundle,
        precision,
        tensorrt_version,
        gpu,
        compute_capability,
        deepstream_version,
        cuda_version,
    )
    destination.mkdir(parents=True, exist_ok=True)
    report: dict[str, object] = {
        "schema": "mbfs.tensorrt-engine-set/v1",
        "bundle": bundle.version,
        "parser_abi": bundle.parser_abi,
        "precision": precision,
        "batch_profiles": {
            model.role: {
                "dynamic": model.dynamic_batch,
                "minimum": model.batch_minimum,
                "optimal": model.batch_optimal,
                "maximum": model.batch_maximum,
            }
            for model in bundle.models
        },
        "trtexec": version,
        "tensorrt": tensorrt_version,
        "deepstream": deepstream_version,
        "cuda": cuda_version,
        "gpu": gpu,
        "gpu_compute_capability": compute_capability,
        "workspace_mib": workspace_mib,
        "engines": {},
    }
    for model in bundle.models:
        engine = destination / f"{model.role}.engine"
        command = [
            executable,
            f"--onnx={model.source}",
            f"--saveEngine={engine}",
            f"--memPoolSize=workspace:{workspace_mib}M",
            "--builderOptimizationLevel=3",
            "--skipInference",
            f"--{precision}",
        ]
        if model.dynamic_batch:
            command.extend(
                [
                    f"--minShapes={_profile_shape(model, model.batch_minimum)}",
                    f"--optShapes={_profile_shape(model, model.batch_optimal)}",
                    f"--maxShapes={_profile_shape(model, model.batch_maximum)}",
                ]
            )
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
            "batch": {
                "dynamic": model.dynamic_batch,
                "minimum": model.batch_minimum,
                "optimal": model.batch_optimal,
                "maximum": model.batch_maximum,
            },
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
