# Event forwarder

This independently deployed service consumes ALPR events and sends them to an HTTP
backend. Delivery is at-least-once. `event_id` is sent as `Idempotency-Key`; the target
backend must make repeated requests safe.

Required environment:

- `EVENT_FORWARDER_BROKERS`
- `EVENT_FORWARDER_BACKEND_URL`

Optional environment includes `EVENT_FORWARDER_BACKEND_TOKEN`, group ID, retry count,
backoff, timeout, and contract root. Inject the token through the deployment secret
mechanism; never place it in a tracked environment file or command line.

The consumer commits only after delivery or successful publication to retry/DLQ.
The retry topic is a bounded parking-lot handoff and is not consumed automatically;
replay is an explicit operator action, preventing an immediate infinite retry loop.

The service exposes `/live`, `/ready`, and `/metrics` on port 8080 by default.

## Test and build

Run these commands from the repository root. The tests use fakes and do not require a
running broker or HTTP backend.

```bash
python3 -m venv services/event-forwarder/.venv
services/event-forwarder/.venv/bin/pip install \
  -e 'services/event-forwarder[test]'
services/event-forwarder/.venv/bin/python -m pytest \
  services/event-forwarder/tests

docker build -f services/event-forwarder/Dockerfile \
  -t mbfs-event-forwarder:local .
```

## Local operation

Start the repository Kafka stack first as documented in `kafka/README.md`. The
forwarder is a normal Kafka client, so its in-network address uses colon syntax
`kafka:29092` (unlike DeepStream's `kafka;29092`). Set the backend URL to a real or mock
HTTP endpoint reachable from the container. This example assumes that endpoint runs on
the Docker host at port 18082:

```bash
docker run -d --name mbfs-event-forwarder-local \
  --network kafka_default \
  --add-host host.docker.internal:host-gateway \
  -p 18081:8080 \
  -e EVENT_FORWARDER_BROKERS=kafka:29092 \
  -e EVENT_FORWARDER_BACKEND_URL=http://host.docker.internal:18082/events \
  mbfs-event-forwarder:local
```

Port 18081 is the host-side development port; port 8080 remains the container default.
For a backend container already attached to `kafka_default`, use its Docker DNS name
instead of `host.docker.internal`. Inject `EVENT_FORWARDER_BACKEND_TOKEN` through the
deployment secret mechanism when required.

Smoke-test process health and metrics:

```bash
curl --fail http://localhost:18081/live
curl --fail http://localhost:18081/ready
curl --fail http://localhost:18081/metrics
```

`/live` proves the process and health server are alive. `/ready` means the forwarder
has initialized and entered its poll loop; the current implementation does not turn it
into a Kafka metadata probe. Confirm that the service has actually joined Kafka by
describing its default consumer group (allow a few seconds after startup):

```bash
docker compose -f kafka/compose.yaml exec -T kafka \
  /opt/kafka/bin/kafka-consumer-groups.sh \
  --bootstrap-server kafka:29092 \
  --describe --group mbfs-event-forwarder-v1
docker logs --tail 50 mbfs-event-forwarder-local
```

The group description proves broker connectivity and displays topic assignment/lag;
the logs must not show repeated broker connection failures. `/metrics` exposes delivery,
retry, dead-letter, and pending counters. A complete delivery smoke test additionally
requires publishing a contract-valid event and observing it at the configured backend.

Remove only this development container when finished:

```bash
docker rm -f mbfs-event-forwarder-local
```
