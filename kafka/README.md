# Local Kafka

Start a single-node development broker and provision the tracked topics:

```bash
KAFKA_HOST_PORT=9093 docker compose -f kafka/compose.yaml up -d --wait
KAFKA_HOST_PORT=9093 docker compose -f kafka/compose.yaml ps
```

Port 9092 is already owned by another service on this server. Host processes use
`localhost:9093`. Containers joined to the Compose-created `kafka_default` network use
`kafka:29092`; they must not use the host endpoint. DeepStream uses `host;port` syntax,
so its equivalent container setting is `ALPR_KAFKA_BROKERS='kafka;29092'`.

Verify both Compose health and the externally advertised host listener:

```bash
KAFKA_HOST_PORT=9093 docker compose -f kafka/compose.yaml ps
docker run --rm --network host \
  --entrypoint /opt/kafka/bin/kafka-topics.sh apache/kafka:4.1.0 \
  --bootstrap-server localhost:9093 --list
```

The stack is for local development only. Production must use secret injection,
TLS/SASL as required, replicated topics, and infrastructure-managed topic
configuration. Broker data is written to ignored `kafka/data/`.

The intended topic configuration is also recorded in `topics/alpr.yaml` for deployment
automation and review.

Stop only this repository's stack with:

```bash
KAFKA_HOST_PORT=9093 docker compose -f kafka/compose.yaml down
```

Do not stop or reconfigure the unrelated Kafka container bound to host port 9092.
