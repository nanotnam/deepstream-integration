from __future__ import annotations

from collections.abc import Iterator

from confluent_kafka import Consumer, KafkaError, Producer

from .forwarder import Record


class KafkaClient:
    def __init__(self, brokers: str, group_id: str, topics: list[str]):
        self._consumer = Consumer(
            {
                "bootstrap.servers": brokers,
                "group.id": group_id,
                "enable.auto.commit": False,
                "auto.offset.reset": "earliest",
            }
        )
        self._producer = Producer({"bootstrap.servers": brokers, "enable.idempotence": True})
        self._consumer.subscribe(topics)

    def records(self) -> Iterator[tuple[object, Record]]:
        while True:
            message = self._consumer.poll(1.0)
            if message is None:
                continue
            if message.error():
                if message.error().code() == KafkaError._PARTITION_EOF:
                    continue
                raise RuntimeError(str(message.error()))
            key = message.key().decode("utf-8", errors="replace") if message.key() else ""
            yield message, Record(message.topic(), key, message.value())

    def publish(
        self, topic: str, key: str, value: bytes, headers: dict[str, str]
    ) -> bool:
        delivered: list[bool] = []
        self._producer.produce(
            topic,
            key=key.encode("utf-8"),
            value=value,
            headers=list(headers.items()),
            on_delivery=lambda error, _: delivered.append(error is None),
        )
        self._producer.flush(10.0)
        return delivered == [True]

    def commit(self, message: object) -> None:
        self._consumer.commit(message=message, asynchronous=False)

    def close(self) -> None:
        self._producer.flush(10.0)
        self._consumer.close()
