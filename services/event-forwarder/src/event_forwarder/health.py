from __future__ import annotations

import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Metrics:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._ready = False
        self._counts: dict[str, int] = {
            "delivered": 0,
            "retried": 0,
            "dead-lettered": 0,
            "pending": 0,
        }

    def ready(self, value: bool) -> None:
        with self._lock:
            self._ready = value

    def record(self, outcome: str) -> None:
        with self._lock:
            self._counts[outcome] = self._counts.get(outcome, 0) + 1

    def snapshot(self) -> tuple[bool, dict[str, int]]:
        with self._lock:
            return self._ready, dict(self._counts)


def serve_health(port: int, metrics: Metrics) -> ThreadingHTTPServer:
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
            ready, counts = metrics.snapshot()
            if self.path == "/live":
                self._respond(200, {"status": "live"})
            elif self.path == "/ready":
                self._respond(200 if ready else 503, {"ready": ready})
            elif self.path == "/metrics":
                self._respond(200, counts)
            else:
                self._respond(404, {"error": "not found"})

        def _respond(self, status: int, body: object) -> None:
            payload = json.dumps(body, separators=(",", ":")).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

        def log_message(self, format: str, *args: object) -> None:
            return

    server = ThreadingHTTPServer(("0.0.0.0", port), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server
