# Lightweight Log Processing & Dynamic Buffer Engine

Production-grade system built around a linked-list core in C, with REST API, PostgreSQL persistence, web dashboard, observability metrics, and containerized deployment.

## Business Problem

Modern backend systems ingest bursty event streams and need to process them with low memory overhead while preserving FIFO semantics. Dynamic arrays can suffer from costly reallocation under volatile workloads. This project simulates an enterprise log-processing pipeline that uses a linked list as the in-memory buffer to support predictable insertion/dequeue behavior and deterministic memory cleanup.

## Solution Summary

The system provides:

- C engine for log ingestion and FIFO queue processing
- Linked-list dynamic buffer with configurable capacity/thresholds
- PostgreSQL persistence through native `libpq` in C (no ORM)
- FastAPI REST layer calling a compiled C shared library (`ctypes`)
- Browser dashboard for operational control and live metrics
- Structured logging, metrics snapshots, health checks, graceful shutdown
- Dockerized runtime with database and app containers

## Architecture

### Text Diagram

```text
[Web Dashboard (HTML/JS)]
          |
          v
[FastAPI REST Layer]  <---->  [C Shared Library (engine_api)]
          |                               |
          |                               v
          |                        [Buffer Engine]
          |                        (Linked List FIFO)
          |                               |
          |                               v
          |                        [Queue Processor]
          |                               |
          |                               v
          +----------------------> [Persistence (libpq)] ---> [PostgreSQL]
```

### C Module Breakdown

- `linked_list.c/.h`: doubly-linked queue primitives (push/pop/clear)
- `log_entry.c/.h`: log model, timestamping, payload validation
- `buffer_engine.c/.h`: bounded queue, metrics, memory estimates, JSON snapshot
- `queue_processor.c/.h`: FIFO consumption, latency measurement, persistence orchestration
- `persistence.c/.h`: PostgreSQL connection, schema creation, inserts, ping
- `logger.c/.h`: structured JSON logs with levels (`DEBUG/INFO/ERROR`)
- `config.c/.h`: environment-based configuration loader
- `engine_api.c/.h`: FFI-safe runtime entry points for API
- `main.c`: CLI runner with signal handling and graceful shutdown

## Data Flow

1. Client sends `POST /logs`.
2. FastAPI calls `engine_add_log(...)` in C shared library.
3. C engine validates payload and appends to linked-list buffer.
4. When threshold is reached (or `/process` is called), queue processor dequeues FIFO.
5. Each processed log is inserted into PostgreSQL (`processed_logs`).
6. Metrics snapshots are persisted (`processing_metrics`) and exposed via `/metrics`.
7. Dashboard polls `/health`, `/metrics`, and `/logs` to display real-time state.

## Project Structure

```text
.
├── src/
│   ├── core/
│   │   ├── linked_list.c
│   │   ├── log_entry.c
│   │   ├── buffer_engine.c
│   │   └── queue_processor.c
│   ├── api/
│   │   ├── app.py
│   │   ├── engine_client.py
│   │   └── engine_api.c
│   ├── db/
│   │   └── persistence.c
│   ├── utils/
│   │   ├── logger.c
│   │   └── config.c
│   └── main.c
├── include/
│   ├── linked_list.h
│   ├── log_entry.h
│   ├── buffer_engine.h
│   ├── queue_processor.h
│   ├── persistence.h
│   ├── logger.h
│   ├── config.h
│   └── engine_api.h
├── web/
│   ├── index.html
│   ├── styles.css
│   └── app.js
├── scripts/
│   ├── init.sql
│   └── start.sh
├── tests/
│   ├── test_linked_list.c
│   └── test_buffer_engine.c
├── legacy/academic/
│   ├── idll.h
│   ├── idll.cpp
│   └── main-idll.cpp
├── Dockerfile
├── docker-compose.yml
├── Makefile
├── TECHNICAL_OVERVIEW.md
└── .env.example
```

## API Endpoints

- `POST /logs`
  - body: `{ "level": "INFO", "source": "dashboard", "message": "..." }`
- `GET /logs`
  - returns pending queue snapshot
- `POST /process`
  - body: `{ "max_items": 100 }` (0 = default batch size)
- `GET /metrics`
  - runtime ingestion/processing/error/memory stats
- `GET /health`
  - service and DB status

## Observability Features

- Structured logs with component + level + UTC timestamp
- Metrics tracked in memory and persisted periodically
- Processing latency (`last_processing_ms`)
- Queue depth and memory estimate
- Error counters and database health endpoint

## Performance Considerations

- **Linked list strengths**:
  - O(1) enqueue at tail
  - O(1) dequeue at head
  - no contiguous-memory reallocation during growth
- **Trade-offs**:
  - pointer overhead per node
  - weaker cache locality compared with arrays
- **Current strategy**:
  - bounded queue (`BUFFER_CAPACITY`) to control memory
  - configurable batch processing (`PROCESS_BATCH_SIZE`)
  - auto-processing threshold (`AUTO_PROCESS_THRESHOLD`) for back-pressure

## Linked List vs Dynamic Array Trade-offs

| Concern | Linked List | Dynamic Array |
|---|---|---|
| Append under burst | Stable O(1) | Amortized O(1), occasional resize spikes |
| Pop from front | O(1) | O(n) unless ring-buffer strategy |
| Memory locality | Lower | Higher |
| Node overhead | Higher (pointers) | Lower per item |
| Predictable FIFO semantics | Excellent | Good with extra queue mechanics |

For this queue-centric ingestion pipeline, linked list gives operational simplicity and stable FIFO operations.

## Scalability Discussion

Current implementation is single-process in-memory queue with DB-backed persistence. Scaling paths:

1. Multi-worker API with one queue per worker + external broker.
2. Replace in-memory queue with distributed queue (Kafka/RabbitMQ).
3. Introduce partitioned processing by source/tenant.
4. Add read replicas + partitioning in PostgreSQL for retention at scale.
5. Export metrics to Prometheus/OpenTelemetry.

## Local Run (Without Docker)

Prerequisites:

- GCC/Clang with C11 support
- PostgreSQL client headers/libs (`libpq`)
- Python 3.11+
- Running PostgreSQL instance

Steps:

```bash
cp .env.example .env
# export env vars manually or via shell tooling

make build
make test
ENGINE_LIB_PATH=build/liblog_engine.so uvicorn src.api.app:app --host 0.0.0.0 --port 8000
```

Then open: `http://localhost:8000`

## Docker Deployment

```bash
cp .env.example .env
docker-compose up --build
```

Services:

- API + Dashboard: `http://localhost:8000`
- PostgreSQL: `localhost:5432`

The C engine auto-creates schema at startup, and `scripts/init.sql` also initializes tables for first boot.

## Graceful Shutdown

- API shutdown triggers `engine_shutdown()`.
- Remaining queue is processed in batches before teardown.
- DB connection is closed cleanly.
- CLI mode (`make run-engine`) handles `SIGINT` and `SIGTERM`.

## Tests

- `tests/test_linked_list.c`: list ordering and FIFO behavior
- `tests/test_buffer_engine.c`: capacity enforcement, metrics, JSON preview

Run:

```bash
make test
```

## Future Improvements

1. Add concurrent worker pool with lock-free queue partitioning.
2. Add retries/dead-letter queue for transient DB failures.
3. Add OpenTelemetry traces and Prometheus metrics endpoint.
4. Add authentication and RBAC on API endpoints.
5. Add retention jobs and archival policies in PostgreSQL.
