# ADR 0001: Application and integration boundaries

Status: accepted

DeepStream applications own their domain code, model contracts, SDK adapters, tests,
and packaging below `apps/<app>`. Code is promoted to `libs/` only after it is useful
outside one application. Shared libraries cannot depend on an application.

Wire schemas live in `contracts/`. Kafka broker assets live in `kafka/`; they contain
no application code. Kafka consumers and external backend adapters are independently
deployable processes below `services/` and cannot depend on DeepStream, CUDA, or
TensorRT.
