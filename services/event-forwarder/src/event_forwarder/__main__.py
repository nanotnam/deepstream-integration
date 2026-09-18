from __future__ import annotations

import logging
import signal

from .config import Config
from .contracts import ContractValidator
from .delivery import HttpDeliveryClient
from .forwarder import EventForwarder, Outcome
from .health import Metrics, serve_health
from .kafka import KafkaClient


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    config = Config.from_environment()
    metrics = Metrics()
    health_server = serve_health(config.health_port, metrics)
    client = KafkaClient(
        config.brokers, config.group_id, [config.event_topic]
    )
    forwarder = EventForwarder(
        ContractValidator(config.contract_root),
        HttpDeliveryClient(
            config.backend_url, config.backend_token, config.request_timeout_seconds
        ),
        client,
        config.retry_topic,
        config.dlq_topic,
        config.maximum_attempts,
        config.initial_backoff_ms,
    )
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt()))
    metrics.ready(True)
    try:
        for kafka_message, record in client.records():
            outcome = forwarder.handle(record)
            if outcome != Outcome.PENDING:
                client.commit(kafka_message)
            metrics.record(outcome)
            logging.info("event outcome=%s topic=%s key=%s", outcome, record.topic, record.key)
    except KeyboardInterrupt:
        return 0
    finally:
        metrics.ready(False)
        health_server.shutdown()
        client.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
