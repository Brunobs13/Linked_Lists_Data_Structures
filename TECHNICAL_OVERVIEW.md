# Technical Overview

## Architecture Explanation

This system separates responsibilities into clear runtime layers:

- **Ingestion layer**: FastAPI endpoints receive external requests.
- **Execution layer**: C shared library exposes deterministic engine functions.
- **Buffer layer**: linked-list queue stores pending logs FIFO.
- **Processing layer**: queue processor dequeues and enriches metrics.
- **Persistence layer**: PostgreSQL writes through native `libpq`.
- **Presentation layer**: dashboard polls API for health and metrics.

A process-wide singleton runtime is used intentionally at the FFI boundary so every API request operates on the same in-memory buffer state.

## Trade-off Analysis

### Why linked list here

- constant-time head/tail operations simplify FIFO queues
- no reallocation spikes under burst ingestion
- explicit node lifecycle gives direct memory control in C

### What is sacrificed

- lower CPU cache locality than contiguous arrays
- pointer overhead increases per-item memory footprint
- random access is O(n)

For queue-first workloads, these trade-offs are acceptable.

## Scaling Discussion

Current architecture is suitable for single-node workloads and controlled throughput. Scaling approaches:

- horizontal API scaling with externalized queue
- decouple processor into dedicated workers
- add idempotent write keys for exactly-once semantics
- introduce partitioning and retention management in PostgreSQL

## Memory Management Explanation

- each ingested log allocates one `LogEntry` and one linked-list node
- dequeue path frees entries after successful DB persistence
- requeue path is used when persistence fails to avoid message loss
- shutdown path drains and clears queue to avoid leaks
- bounded buffer (`BUFFER_CAPACITY`) prevents unbounded allocation

## Concurrency Explanation

- queue internals are protected via `pthread_mutex_t`
- API entry points serialize critical runtime operations via a runtime mutex
- current mode is safe for one-process execution
- next step for high concurrency: partitioned queues + dedicated processor threads

## Junior-Level Interview Questions

1. Why is a linked list a good structure for FIFO processing?
2. What is the difference between enqueue and dequeue complexity in this design?
3. Why do we use environment variables instead of hardcoded DB credentials?
4. What does `libpq` do in this project?
5. Why do we need graceful shutdown in backend services?
6. How does the health endpoint help operations teams?
7. Why do we persist metrics as well as processed logs?

## Senior-Level Interview Questions

1. How would you redesign this for 100k logs/second ingestion?
2. How would you guarantee idempotency and prevent duplicate writes?
3. How would you isolate failures between ingestion and persistence tiers?
4. What are the implications of singleton runtime in multi-worker deployments?
5. How would you introduce exactly-once or at-least-once delivery guarantees?
6. How would you instrument p95/p99 latency and error budgets?
7. What would your migration strategy be from in-memory queue to Kafka?
8. How would you secure this API for production multi-tenant environments?

## Production Hardening Checklist

- add authentication and authorization
- add request validation limits and rate limiting
- add TLS termination and secure secrets management
- add retries with backoff and dead-letter handling
- add tracing and centralized log aggregation
- add chaos tests for DB outages and slow writes
