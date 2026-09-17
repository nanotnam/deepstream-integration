from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

from .contracts import ContractError, load_bundle, validate_onnx_bindings
from .engine import build_engines
from .reference import run_reference


def _asset_root(value: str | None) -> Path:
    selected = value or os.environ.get("ALPR_MODEL_ROOT")
    if not selected:
        raise ContractError("model asset root is required via --asset-root or ALPR_MODEL_ROOT")
    return Path(selected)


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(prog="ds-model")
    commands = root.add_subparsers(dest="command", required=True)
    validate = commands.add_parser("validate")
    validate.add_argument("--bundle", type=Path, required=True)
    validate.add_argument("--asset-root")
    validate.add_argument("--contracts-only", action="store_true")

    build = commands.add_parser("build")
    build.add_argument("--bundle", type=Path, required=True)
    build.add_argument("--asset-root")
    build.add_argument("--precision", choices=("fp16", "int8"), required=True)
    build.add_argument("--output", type=Path, default=Path(".local/engines"))
    build.add_argument("--workspace-mib", type=int, default=2048)
    build.add_argument("--calibration-cache-dir", type=Path)
    build.add_argument("--trtexec", default="trtexec")

    compare = commands.add_parser("compare")
    compare.add_argument("--reference", choices=("onnx",), required=True)
    compare.add_argument("--engine", type=Path, required=True)
    compare.add_argument("--bundle", type=Path, required=True)
    compare.add_argument("--asset-root")

    reference = commands.add_parser("reference")
    reference.add_argument("--bundle", type=Path, required=True)
    reference.add_argument("--asset-root")
    reference.add_argument("--role", choices=("vehicle", "plate", "lprnet"), required=True)
    reference.add_argument("--image", type=Path, required=True)
    reference.add_argument("--expected")
    return root


def main(argv: list[str] | None = None) -> int:
    arguments = parser().parse_args(argv)
    try:
        bundle = load_bundle(arguments.bundle, _asset_root(arguments.asset_root))
        if arguments.command == "validate":
            report: dict[str, object] = {
                "schema": "mbfs.model-validation/v1",
                "bundle": bundle.version,
                "hashes": "valid",
            }
            if not arguments.contracts_only:
                report["onnx"] = validate_onnx_bindings(bundle)
            print(json.dumps(report, indent=2, sort_keys=True))
            return 0
        if arguments.command == "build":
            destination = build_engines(
                bundle=bundle,
                precision=arguments.precision,
                output_root=arguments.output,
                workspace_mib=arguments.workspace_mib,
                calibration_cache_dir=arguments.calibration_cache_dir,
                trtexec=arguments.trtexec,
            )
            print(destination)
            return 0
        if arguments.command == "reference":
            report = run_reference(bundle, arguments.role, arguments.image)
            if arguments.expected is not None:
                report["expected"] = arguments.expected
                report["exact"] = report.get("text") == arguments.expected
                if not report["exact"]:
                    print(json.dumps(report, indent=2, sort_keys=True))
                    raise ContractError(
                        f"decoded {report.get('text')!r}, expected {arguments.expected!r}"
                    )
            print(json.dumps(report, indent=2, sort_keys=True))
            return 0
        if not arguments.engine.is_file():
            raise ContractError(f"TensorRT engine is missing: {arguments.engine}")
        validate_onnx_bindings(bundle)
        raise ContractError(
            "numerical comparison requires the GPU-side TensorRT runner and is intentionally "
            "deferred until server qualification"
        )
    except ContractError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
