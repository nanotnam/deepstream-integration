from __future__ import annotations

import json
import urllib.error
import urllib.request
from typing import Any


class TransientDeliveryError(RuntimeError):
    pass


class PermanentDeliveryError(RuntimeError):
    pass


class HttpDeliveryClient:
    def __init__(self, url: str, token: str, timeout_seconds: float):
        self._url = url
        self._token = token
        self._timeout_seconds = timeout_seconds

    def deliver(self, event: dict[str, Any]) -> None:
        headers = {
            "Content-Type": "application/json",
            "Idempotency-Key": str(event["event_id"]),
        }
        if self._token:
            headers["Authorization"] = f"Bearer {self._token}"
        request = urllib.request.Request(
            self._url,
            data=json.dumps(event, separators=(",", ":")).encode("utf-8"),
            headers=headers,
            method="POST",
        )
        try:
            with urllib.request.urlopen(request, timeout=self._timeout_seconds) as response:
                if response.status >= 500:
                    raise TransientDeliveryError(f"backend returned {response.status}")
                if response.status >= 400:
                    raise PermanentDeliveryError(f"backend returned {response.status}")
        except urllib.error.HTTPError as exc:
            if exc.code >= 500 or exc.code in (408, 429):
                raise TransientDeliveryError(f"backend returned {exc.code}") from exc
            raise PermanentDeliveryError(f"backend returned {exc.code}") from exc
        except (urllib.error.URLError, TimeoutError) as exc:
            raise TransientDeliveryError("backend is unavailable") from exc
