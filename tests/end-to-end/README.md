# End-to-end tests

The target flow is synthetic producer to Kafka to event-forwarder to a mock HTTP
backend. The GPU-qualified variant replaces the synthetic producer with Traffic ALPR
file replay. Tests must include broker outage, backend outage, poison records,
duplicates, restart recovery, and consumer lag observation.
