from __future__ import annotations

import json
import time
from dataclasses import dataclass
from typing import Any, Callable, Protocol

from .contracts import ContractError, ContractValidator
from .delivery import PermanentDeliveryError, TransientDeliveryError


class DeliveryClient(Protocol):
    def deliver(self, event: dict[str, Any]) -> None: ...


class Sink(Protocol):
    def publish(self, topic: str, key: str, value: bytes, headers: dict[str, str]) -> bool: ...


@dataclass(frozen=True)
class Record:
    topic: str
    key: str
    value: bytes


class Outcome:
    DELIVERED = "delivered"
    RETRIED = "retried"
    DEAD_LETTERED = "dead-lettered"
    PENDING = "pending"


class EventForwarder:
    def __init__(
        self,
        validator: ContractValidator,
        delivery: DeliveryClient,
        sink: Sink,
        retry_topic: str,
        dlq_topic: str,
        maximum_attempts: int = 3,
        initial_backoff_ms: int = 250,
        sleep: Callable[[float], None] = time.sleep,
    ):
        self._validator = validator
        self._delivery = delivery
        self._sink = sink
        self._retry_topic = retry_topic
        self._dlq_topic = dlq_topic
        self._maximum_attempts = maximum_attempts
        self._initial_backoff_ms = initial_backoff_ms
        self._sleep = sleep

    def handle(self, record: Record) -> str:
        try:
            event = self._validator.decode(record.value)
        except ContractError as exc:
            return self._dead_letter(record, "contract", str(exc))

        for attempt in range(1, self._maximum_attempts + 1):
            try:
                self._delivery.deliver(event)
                return Outcome.DELIVERED
            except PermanentDeliveryError as exc:
                return self._dead_letter(record, "permanent-delivery", str(exc))
            except TransientDeliveryError as exc:
                if attempt == self._maximum_attempts:
                    accepted = self._sink.publish(
                        self._retry_topic,
                        str(event["event_id"]),
                        record.value,
                        {"failure": "transient-delivery", "attempts": str(attempt)},
                    )
                    return Outcome.RETRIED if accepted else Outcome.PENDING
                self._sleep((self._initial_backoff_ms * (2 ** (attempt - 1))) / 1000.0)
        return Outcome.PENDING

    def _dead_letter(self, record: Record, category: str, detail: str) -> str:
        envelope = json.dumps(
            {
                "schema": "mbfs.dead-letter.v1",
                "source_topic": record.topic,
                "source_key": record.key,
                "category": category,
                "detail": detail[:512],
                "payload": record.value.decode("utf-8", errors="replace"),
            },
            separators=(",", ":"),
        ).encode("utf-8")
        accepted = self._sink.publish(
            self._dlq_topic, record.key, envelope, {"failure": category}
        )
        return Outcome.DEAD_LETTERED if accepted else Outcome.PENDING
