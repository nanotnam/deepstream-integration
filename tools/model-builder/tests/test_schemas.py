from __future__ import annotations

import json
from pathlib import Path

from jsonschema import Draft202012Validator, FormatChecker


def test_tracked_payload_examples_match_schemas() -> None:
    root = Path(__file__).resolve().parents[3] / "apps" / "traffic-alpr" / "schemas"
    for name in ("event-v1", "health-v1"):
        schema = json.loads((root / f"{name}.schema.json").read_text(encoding="utf-8"))
        example = json.loads((root / f"{name}.example.json").read_text(encoding="utf-8"))
        Draft202012Validator(schema, format_checker=FormatChecker()).validate(example)

