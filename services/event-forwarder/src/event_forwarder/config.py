from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Config:
    brokers: str
    group_id: str
    backend_url: str
    backend_token: str
    contract_root: Path
    event_topic: str = "mbfs.alpr.events.v1"
    retry_topic: str = "mbfs.alpr.events.retry.v1"
    dlq_topic: str = "mbfs.alpr.events.dlq.v1"
    maximum_attempts: int = 3
    initial_backoff_ms: int = 250
    request_timeout_seconds: float = 10.0
    health_port: int = 8080

    @classmethod
    def from_environment(cls) -> "Config":
        required = ("EVENT_FORWARDER_BROKERS", "EVENT_FORWARDER_BACKEND_URL")
        missing = [name for name in required if not os.environ.get(name)]
        if missing:
            raise ValueError(f"missing required environment: {', '.join(missing)}")
        attempts = int(os.environ.get("EVENT_FORWARDER_MAXIMUM_ATTEMPTS", "3"))
        if attempts < 1:
            raise ValueError("EVENT_FORWARDER_MAXIMUM_ATTEMPTS must be positive")
        return cls(
            brokers=os.environ["EVENT_FORWARDER_BROKERS"],
            group_id=os.environ.get("EVENT_FORWARDER_GROUP_ID", "mbfs-event-forwarder-v1"),
            backend_url=os.environ["EVENT_FORWARDER_BACKEND_URL"],
            backend_token=os.environ.get("EVENT_FORWARDER_BACKEND_TOKEN", ""),
            contract_root=Path(os.environ.get("EVENT_FORWARDER_CONTRACT_ROOT", "/opt/mbfs/contracts")),
            maximum_attempts=attempts,
            initial_backoff_ms=int(os.environ.get("EVENT_FORWARDER_INITIAL_BACKOFF_MS", "250")),
            request_timeout_seconds=float(os.environ.get("EVENT_FORWARDER_REQUEST_TIMEOUT_SECONDS", "10")),
            health_port=int(os.environ.get("EVENT_FORWARDER_HEALTH_PORT", "8080")),
        )
