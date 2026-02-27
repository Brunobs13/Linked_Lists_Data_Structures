#include "persistence.h"

#include <stdio.h>
#include <string.h>

static void write_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s", message);
    }
}

static int exec_command(Persistence *persistence, const char *sql, char *error, size_t error_size) {
    PGresult *result = PQexec(persistence->conn, sql);
    if (result == NULL) {
        write_error(error, error_size, "PQexec returned NULL result.");
        return 0;
    }

    ExecStatusType status = PQresultStatus(result);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        write_error(error, error_size, PQerrorMessage(persistence->conn));
        PQclear(result);
        return 0;
    }

    PQclear(result);
    return 1;
}

int persistence_init(Persistence *persistence, const AppConfig *config, AppLogger *logger, char *error, size_t error_size) {
    if (persistence == NULL || config == NULL) {
        write_error(error, error_size, "Invalid persistence initialization arguments.");
        return 0;
    }

    memset(persistence, 0, sizeof(*persistence));
    persistence->logger = logger;

    char conninfo[512] = {0};
    if (!config_build_conninfo(config, conninfo, sizeof(conninfo))) {
        write_error(error, error_size, "Failed to build PostgreSQL conninfo.");
        return 0;
    }

    persistence->conn = PQconnectdb(conninfo);
    if (persistence->conn == NULL || PQstatus(persistence->conn) != CONNECTION_OK) {
        write_error(error,
                    error_size,
                    persistence->conn != NULL ? PQerrorMessage(persistence->conn) : "PQconnectdb failed.");
        if (persistence->conn != NULL) {
            PQfinish(persistence->conn);
            persistence->conn = NULL;
        }
        return 0;
    }

    const char *schema_sql =
        "CREATE TABLE IF NOT EXISTS processed_logs ("
        " id BIGSERIAL PRIMARY KEY,"
        " log_id BIGINT NOT NULL,"
        " level TEXT NOT NULL,"
        " source TEXT NOT NULL,"
        " message TEXT NOT NULL,"
        " ingested_at TIMESTAMPTZ NOT NULL,"
        " processed_at TIMESTAMPTZ NOT NULL,"
        " processing_ms DOUBLE PRECISION NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS processing_metrics ("
        " id BIGSERIAL PRIMARY KEY,"
        " created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        " total_ingested BIGINT NOT NULL,"
        " total_processed BIGINT NOT NULL,"
        " total_errors BIGINT NOT NULL,"
        " queue_depth BIGINT NOT NULL,"
        " buffer_capacity BIGINT NOT NULL,"
        " memory_bytes BIGINT NOT NULL,"
        " last_processing_ms DOUBLE PRECISION NOT NULL"
        ");";

    if (!exec_command(persistence, schema_sql, error, error_size)) {
        PQfinish(persistence->conn);
        persistence->conn = NULL;
        return 0;
    }

    persistence->initialized = 1;
    logger_log(logger, LOGGER_INFO, "persistence", "postgres connection initialized");
    return 1;
}

int persistence_ping(Persistence *persistence, char *error, size_t error_size) {
    if (persistence == NULL || !persistence->initialized || persistence->conn == NULL) {
        write_error(error, error_size, "Persistence layer is not initialized.");
        return 0;
    }

    PGresult *result = PQexec(persistence->conn, "SELECT 1");
    if (result == NULL) {
        write_error(error, error_size, "Ping query returned NULL result.");
        return 0;
    }

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        write_error(error, error_size, PQerrorMessage(persistence->conn));
        PQclear(result);
        return 0;
    }

    PQclear(result);
    return 1;
}

int persistence_insert_processed_log(Persistence *persistence,
                                     const LogEntry *entry,
                                     int64_t processed_at_ms,
                                     double processing_ms,
                                     char *error,
                                     size_t error_size) {
    if (persistence == NULL || !persistence->initialized || entry == NULL) {
        write_error(error, error_size, "Invalid processed log insert arguments.");
        return 0;
    }

    char id_buf[32] = {0};
    char ingested_buf[64] = {0};
    char processed_buf[64] = {0};
    char latency_buf[64] = {0};

    snprintf(id_buf, sizeof(id_buf), "%llu", (unsigned long long)entry->id);
    snprintf(ingested_buf, sizeof(ingested_buf), "%.6f", ((double)entry->ingested_at_ms) / 1000.0);
    snprintf(processed_buf, sizeof(processed_buf), "%.6f", ((double)processed_at_ms) / 1000.0);
    snprintf(latency_buf, sizeof(latency_buf), "%.3f", processing_ms);

    const char *params[7] = {
        id_buf,
        entry->level,
        entry->source,
        entry->message,
        ingested_buf,
        processed_buf,
        latency_buf,
    };

    const char *sql =
        "INSERT INTO processed_logs "
        "(log_id, level, source, message, ingested_at, processed_at, processing_ms) "
        "VALUES ($1::BIGINT, $2, $3, $4, to_timestamp($5::DOUBLE PRECISION), "
        "to_timestamp($6::DOUBLE PRECISION), $7::DOUBLE PRECISION)";

    PGresult *result = PQexecParams(persistence->conn, sql, 7, NULL, params, NULL, NULL, 0);
    if (result == NULL) {
        write_error(error, error_size, "Processed log insert returned NULL result.");
        return 0;
    }

    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        write_error(error, error_size, PQerrorMessage(persistence->conn));
        PQclear(result);
        return 0;
    }

    PQclear(result);
    return 1;
}

int persistence_insert_metrics(Persistence *persistence,
                               const EngineMetrics *metrics,
                               char *error,
                               size_t error_size) {
    if (persistence == NULL || !persistence->initialized || metrics == NULL) {
        write_error(error, error_size, "Invalid metrics insert arguments.");
        return 0;
    }

    char ingested_buf[32] = {0};
    char processed_buf[32] = {0};
    char errors_buf[32] = {0};
    char depth_buf[32] = {0};
    char capacity_buf[32] = {0};
    char memory_buf[32] = {0};
    char last_processing_buf[64] = {0};

    snprintf(ingested_buf, sizeof(ingested_buf), "%llu", (unsigned long long)metrics->total_ingested);
    snprintf(processed_buf, sizeof(processed_buf), "%llu", (unsigned long long)metrics->total_processed);
    snprintf(errors_buf, sizeof(errors_buf), "%llu", (unsigned long long)metrics->total_errors);
    snprintf(depth_buf, sizeof(depth_buf), "%zu", metrics->queue_depth);
    snprintf(capacity_buf, sizeof(capacity_buf), "%zu", metrics->buffer_capacity);
    snprintf(memory_buf, sizeof(memory_buf), "%zu", metrics->memory_bytes_estimate);
    snprintf(last_processing_buf, sizeof(last_processing_buf), "%.3f", metrics->last_processing_ms);

    const char *params[7] = {
        ingested_buf,
        processed_buf,
        errors_buf,
        depth_buf,
        capacity_buf,
        memory_buf,
        last_processing_buf,
    };

    const char *sql =
        "INSERT INTO processing_metrics "
        "(total_ingested, total_processed, total_errors, queue_depth, buffer_capacity, memory_bytes, last_processing_ms) "
        "VALUES ($1::BIGINT, $2::BIGINT, $3::BIGINT, $4::BIGINT, $5::BIGINT, $6::BIGINT, $7::DOUBLE PRECISION)";

    PGresult *result = PQexecParams(persistence->conn, sql, 7, NULL, params, NULL, NULL, 0);
    if (result == NULL) {
        write_error(error, error_size, "Metrics insert returned NULL result.");
        return 0;
    }

    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        write_error(error, error_size, PQerrorMessage(persistence->conn));
        PQclear(result);
        return 0;
    }

    PQclear(result);
    return 1;
}

void persistence_close(Persistence *persistence) {
    if (persistence == NULL) {
        return;
    }

    if (persistence->conn != NULL) {
        PQfinish(persistence->conn);
        persistence->conn = NULL;
    }

    persistence->initialized = 0;
}
