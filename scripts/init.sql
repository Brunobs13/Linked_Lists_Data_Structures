CREATE TABLE IF NOT EXISTS processed_logs (
    id BIGSERIAL PRIMARY KEY,
    log_id BIGINT NOT NULL,
    level TEXT NOT NULL,
    source TEXT NOT NULL,
    message TEXT NOT NULL,
    ingested_at TIMESTAMPTZ NOT NULL,
    processed_at TIMESTAMPTZ NOT NULL,
    processing_ms DOUBLE PRECISION NOT NULL
);

CREATE TABLE IF NOT EXISTS processing_metrics (
    id BIGSERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    total_ingested BIGINT NOT NULL,
    total_processed BIGINT NOT NULL,
    total_errors BIGINT NOT NULL,
    queue_depth BIGINT NOT NULL,
    buffer_capacity BIGINT NOT NULL,
    memory_bytes BIGINT NOT NULL,
    last_processing_ms DOUBLE PRECISION NOT NULL
);
