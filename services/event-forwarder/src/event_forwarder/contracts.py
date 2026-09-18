from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from jsonschema import Draft202012Validator, FormatChecker


class ContractError(ValueError):
    pass


class ContractValidator:
    def __init__(self, contract_root: Path):
        schema_path = contract_root / "alpr" / "event" / "v1" / "schema.json"
        try:
            schema = json.loads(schema_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise ContractError(f"cannot load event contract: {schema_path}") from exc
        self._validator = Draft202012Validator(schema, format_checker=FormatChecker())

    def decode(self, payload: bytes) -> dict[str, Any]:
        try:
            event = json.loads(payload)
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise ContractError("payload is not valid UTF-8 JSON") from exc
        if not isinstance(event, dict):
            raise ContractError("payload must be a JSON object")
        errors = sorted(self._validator.iter_errors(event), key=lambda item: list(item.path))
        if errors:
            raise ContractError(errors[0].message)
        return event
