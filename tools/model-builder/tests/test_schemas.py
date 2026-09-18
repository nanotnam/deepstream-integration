from __future__ import annotations

import json
from pathlib import Path

from jsonschema import Draft202012Validator, FormatChecker


def test_tracked_payload_examples_match_schemas() -> None:
    root = Path(__file__).resolve().parents[3] / "contracts" / "alpr"
    for name in ("event", "health"):
        schema = json.loads((root / name / "v1" / "schema.json").read_text(encoding="utf-8"))
        example = json.loads((root / name / "v1" / "example.json").read_text(encoding="utf-8"))
        Draft202012Validator(schema, format_checker=FormatChecker()).validate(example)

    common = root.parent / "common" / "dead-letter" / "v1"
    schema = json.loads((common / "schema.json").read_text(encoding="utf-8"))
    example = json.loads((common / "example.json").read_text(encoding="utf-8"))
    Draft202012Validator(schema, format_checker=FormatChecker()).validate(example)
