from __future__ import annotations

import json
from pathlib import Path

from event_forwarder.contracts import ContractValidator
from event_forwarder.delivery import PermanentDeliveryError, TransientDeliveryError
from event_forwarder.forwarder import EventForwarder, Outcome, Record
from event_forwarder.health import Metrics


ROOT = Path(__file__).resolve().parents[3]
EXAMPLE = (ROOT / "contracts/alpr/event/v1/example.json").read_bytes()


class Delivery:
    def __init__(self, failures: list[Exception] | None = None):
        self.failures = list(failures or [])
        self.events: list[dict[str, object]] = []

    def deliver(self, event: dict[str, object]) -> None:
        if self.failures:
            raise self.failures.pop(0)
        self.events.append(event)


class Sink:
    def __init__(self, accepted: bool = True):
        self.accepted = accepted
        self.messages: list[tuple[str, str, bytes, dict[str, str]]] = []

    def publish(self, topic: str, key: str, value: bytes, headers: dict[str, str]) -> bool:
        self.messages.append((topic, key, value, headers))
        return self.accepted


def subject(delivery: Delivery, sink: Sink, attempts: int = 3) -> EventForwarder:
    return EventForwarder(
        ContractValidator(ROOT / "contracts"), delivery, sink,
        "retry", "dlq", attempts, 1, sleep=lambda _: None,
    )


def test_valid_event_is_delivered() -> None:
    delivery, sink = Delivery(), Sink()
    assert subject(delivery, sink).handle(Record("events", "camera", EXAMPLE)) == Outcome.DELIVERED
    assert len(delivery.events) == 1
    assert sink.messages == []


def test_invalid_event_is_dead_lettered() -> None:
    sink = Sink()
    outcome = subject(Delivery(), sink).handle(Record("events", "bad", b"{}"))
    assert outcome == Outcome.DEAD_LETTERED
    envelope = json.loads(sink.messages[0][2])
    assert envelope["category"] == "contract"


def test_transient_failure_enters_retry_after_bound() -> None:
    errors = [TransientDeliveryError("down"), TransientDeliveryError("down")]
    sink = Sink()
    outcome = subject(Delivery(errors), sink, attempts=2).handle(
        Record("events", "camera", EXAMPLE)
    )
    assert outcome == Outcome.RETRIED
    assert sink.messages[0][0] == "retry"


def test_failed_dlq_write_keeps_offset_pending() -> None:
    sink = Sink(accepted=False)
    outcome = subject(Delivery([PermanentDeliveryError("rejected")]), sink).handle(
        Record("events", "camera", EXAMPLE)
    )
    assert outcome == Outcome.PENDING


def test_health_metrics_track_readiness_and_outcomes() -> None:
    metrics = Metrics()
    assert metrics.snapshot() == (False, {
        "delivered": 0, "retried": 0, "dead-lettered": 0, "pending": 0
    })
    metrics.ready(True)
    metrics.record(Outcome.DELIVERED)
    ready, counts = metrics.snapshot()
    assert ready
    assert counts["delivered"] == 1
