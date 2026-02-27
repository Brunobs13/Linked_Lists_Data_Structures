#include "engine_api.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "buffer_engine.h"
#include "config.h"
#include "log_entry.h"
#include "persistence.h"
#include "queue_processor.h"

#define ENGINE_ERROR_BUFFER_SIZE 512
#define ENGINE_JSON_SMALL 4096
#define ENGINE_JSON_PENDING 262144

/*
 * Process-wide runtime singleton is justified here because the API wrapper
 * (FFI boundary) needs one shared in-memory queue across HTTP requests.
 */
typedef struct {
    int initialized;
    AppConfig config;
    AppLogger logger;
    BufferEngine buffer;
    Persistence persistence;
    QueueProcessor processor;
    pthread_mutex_t lock;
    char last_error[ENGINE_ERROR_BUFFER_SIZE];
    char json_metrics[ENGINE_JSON_SMALL];
    char json_health[ENGINE_JSON_SMALL];
    char json_process[ENGINE_JSON_SMALL];
    char json_pending[ENGINE_JSON_PENDING];
} EngineRuntime;

static EngineRuntime g_runtime = {
    .initialized = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER,
};

static void set_last_error(const char *error_text) {
    snprintf(g_runtime.last_error,
             sizeof(g_runtime.last_error),
             "%s",
             error_text != NULL ? error_text : "unknown error");

    if (g_runtime.logger.initialized) {
        logger_log(&g_runtime.logger, LOGGER_ERROR, "engine_api", "%s", g_runtime.last_error);
    }
}

static int ensure_initialized(void) {
    if (!g_runtime.initialized) {
        set_last_error("engine runtime is not initialized");
        return 0;
    }

    return 1;
}

int engine_init(void) {
    pthread_mutex_lock(&g_runtime.lock);

    if (g_runtime.initialized) {
        pthread_mutex_unlock(&g_runtime.lock);
        return 1;
    }

    char error[ENGINE_ERROR_BUFFER_SIZE] = {0};

    if (!config_load_from_env(&g_runtime.config, error, sizeof(error))) {
        set_last_error(error);
        pthread_mutex_unlock(&g_runtime.lock);
        return 0;
    }

    if (!logger_init(&g_runtime.logger, g_runtime.config.log_level, stdout)) {
        set_last_error("failed to initialize logger");
        pthread_mutex_unlock(&g_runtime.lock);
        return 0;
    }

    if (!buffer_engine_init(&g_runtime.buffer,
                            g_runtime.config.buffer_capacity,
                            &g_runtime.logger,
                            error,
                            sizeof(error))) {
        set_last_error(error);
        logger_close(&g_runtime.logger);
        pthread_mutex_unlock(&g_runtime.lock);
        return 0;
    }

    if (!persistence_init(&g_runtime.persistence,
                          &g_runtime.config,
                          &g_runtime.logger,
                          error,
                          sizeof(error))) {
        set_last_error(error);
        buffer_engine_shutdown(&g_runtime.buffer);
        logger_close(&g_runtime.logger);
        pthread_mutex_unlock(&g_runtime.lock);
        return 0;
    }

    if (!queue_processor_init(&g_runtime.processor,
                              &g_runtime.buffer,
                              &g_runtime.persistence,
                              &g_runtime.logger,
                              g_runtime.config.process_batch_size,
                              error,
                              sizeof(error))) {
        set_last_error(error);
        persistence_close(&g_runtime.persistence);
        buffer_engine_shutdown(&g_runtime.buffer);
        logger_close(&g_runtime.logger);
        pthread_mutex_unlock(&g_runtime.lock);
        return 0;
    }

    g_runtime.initialized = 1;
    snprintf(g_runtime.last_error, sizeof(g_runtime.last_error), "");
    logger_log(&g_runtime.logger, LOGGER_INFO, "engine_api", "runtime initialized");

    pthread_mutex_unlock(&g_runtime.lock);
    return 1;
}

int engine_shutdown(void) {
    pthread_mutex_lock(&g_runtime.lock);

    if (!g_runtime.initialized) {
        pthread_mutex_unlock(&g_runtime.lock);
        return 1;
    }

    for (;;) {
        size_t processed = 0;
        double elapsed = 0.0;
        char error[ENGINE_ERROR_BUFFER_SIZE] = {0};

        if (!queue_processor_process(&g_runtime.processor,
                                     g_runtime.config.process_batch_size,
                                     &processed,
                                     &elapsed,
                                     error,
                                     sizeof(error))) {
            set_last_error(error);
            break;
        }

        if (processed == 0) {
            break;
        }
    }

    persistence_close(&g_runtime.persistence);
    buffer_engine_shutdown(&g_runtime.buffer);
    logger_log(&g_runtime.logger, LOGGER_INFO, "engine_api", "runtime shutdown completed");
    logger_close(&g_runtime.logger);

    g_runtime.initialized = 0;
    pthread_mutex_unlock(&g_runtime.lock);
    return 1;
}

int engine_add_log(const char *level, const char *message, const char *source) {
    pthread_mutex_lock(&g_runtime.lock);

    if (!ensure_initialized()) {
        pthread_mutex_unlock(&g_runtime.lock);
        return 0;
    }

    const char *resolved_level = (level != NULL && level[0] != '\0') ? level : "INFO";
    const char *resolved_source = (source != NULL && source[0] != '\0') ? source : "api";

    char error[ENGINE_ERROR_BUFFER_SIZE] = {0};
    if (!buffer_engine_enqueue(&g_runtime.buffer,
                               resolved_level,
                               resolved_source,
                               message,
                               error,
                               sizeof(error))) {
        set_last_error(error);
        pthread_mutex_unlock(&g_runtime.lock);
        return 0;
    }

    EngineMetrics metrics;
    if (buffer_engine_get_metrics(&g_runtime.buffer, &metrics) &&
        metrics.queue_depth >= g_runtime.config.auto_process_threshold) {
        size_t processed = 0;
        double elapsed = 0.0;
        if (!queue_processor_process(&g_runtime.processor,
                                     g_runtime.config.process_batch_size,
                                     &processed,
                                     &elapsed,
                                     error,
                                     sizeof(error))) {
            set_last_error(error);
            pthread_mutex_unlock(&g_runtime.lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&g_runtime.lock);
    return 1;
}

const char *engine_get_pending_logs(void) {
    pthread_mutex_lock(&g_runtime.lock);

    if (!ensure_initialized()) {
        snprintf(g_runtime.json_pending,
                 sizeof(g_runtime.json_pending),
                 "{\"error\":\"%s\"}",
                 g_runtime.last_error);
        pthread_mutex_unlock(&g_runtime.lock);
        return g_runtime.json_pending;
    }

    buffer_engine_pending_json(&g_runtime.buffer,
                               g_runtime.config.pending_preview_limit,
                               g_runtime.json_pending,
                               sizeof(g_runtime.json_pending));

    pthread_mutex_unlock(&g_runtime.lock);
    return g_runtime.json_pending;
}

const char *engine_process_queue(size_t max_items) {
    pthread_mutex_lock(&g_runtime.lock);

    if (!ensure_initialized()) {
        snprintf(g_runtime.json_process,
                 sizeof(g_runtime.json_process),
                 "{\"status\":\"error\",\"error\":\"%s\"}",
                 g_runtime.last_error);
        pthread_mutex_unlock(&g_runtime.lock);
        return g_runtime.json_process;
    }

    size_t processed = 0;
    double elapsed_ms = 0.0;
    char error[ENGINE_ERROR_BUFFER_SIZE] = {0};

    if (!queue_processor_process(&g_runtime.processor,
                                 max_items,
                                 &processed,
                                 &elapsed_ms,
                                 error,
                                 sizeof(error))) {
        set_last_error(error);
        snprintf(g_runtime.json_process,
                 sizeof(g_runtime.json_process),
                 "{\"status\":\"error\",\"error\":\"%s\"}",
                 g_runtime.last_error);
        pthread_mutex_unlock(&g_runtime.lock);
        return g_runtime.json_process;
    }

    snprintf(g_runtime.json_process,
             sizeof(g_runtime.json_process),
             "{\"status\":\"ok\",\"processed\":%zu,\"elapsed_ms\":%.3f}",
             processed,
             elapsed_ms);

    pthread_mutex_unlock(&g_runtime.lock);
    return g_runtime.json_process;
}

const char *engine_get_metrics(void) {
    pthread_mutex_lock(&g_runtime.lock);

    if (!ensure_initialized()) {
        snprintf(g_runtime.json_metrics,
                 sizeof(g_runtime.json_metrics),
                 "{\"error\":\"%s\"}",
                 g_runtime.last_error);
        pthread_mutex_unlock(&g_runtime.lock);
        return g_runtime.json_metrics;
    }

    EngineMetrics metrics;
    if (!buffer_engine_get_metrics(&g_runtime.buffer, &metrics)) {
        set_last_error("failed to read metrics");
        snprintf(g_runtime.json_metrics,
                 sizeof(g_runtime.json_metrics),
                 "{\"error\":\"%s\"}",
                 g_runtime.last_error);
        pthread_mutex_unlock(&g_runtime.lock);
        return g_runtime.json_metrics;
    }

    int64_t now_ms = log_entry_now_ms();
    double uptime_seconds = 0.0;
    if (now_ms > metrics.started_at_ms) {
        uptime_seconds = (double)(now_ms - metrics.started_at_ms) / 1000.0;
    }

    snprintf(g_runtime.json_metrics,
             sizeof(g_runtime.json_metrics),
             "{\"total_ingested\":%llu,\"total_processed\":%llu,\"total_errors\":%llu,"
             "\"queue_depth\":%zu,\"buffer_capacity\":%zu,\"memory_bytes_estimate\":%zu,"
             "\"last_processing_ms\":%.3f,\"uptime_seconds\":%.3f}",
             (unsigned long long)metrics.total_ingested,
             (unsigned long long)metrics.total_processed,
             (unsigned long long)metrics.total_errors,
             metrics.queue_depth,
             metrics.buffer_capacity,
             metrics.memory_bytes_estimate,
             metrics.last_processing_ms,
             uptime_seconds);

    pthread_mutex_unlock(&g_runtime.lock);
    return g_runtime.json_metrics;
}

const char *engine_health(void) {
    pthread_mutex_lock(&g_runtime.lock);

    if (!ensure_initialized()) {
        snprintf(g_runtime.json_health,
                 sizeof(g_runtime.json_health),
                 "{\"status\":\"down\",\"error\":\"%s\"}",
                 g_runtime.last_error);
        pthread_mutex_unlock(&g_runtime.lock);
        return g_runtime.json_health;
    }

    char error[ENGINE_ERROR_BUFFER_SIZE] = {0};
    int db_ok = persistence_ping(&g_runtime.persistence, error, sizeof(error));

    EngineMetrics metrics;
    buffer_engine_get_metrics(&g_runtime.buffer, &metrics);

    snprintf(g_runtime.json_health,
             sizeof(g_runtime.json_health),
             "{\"status\":\"%s\",\"db\":\"%s\",\"queue_depth\":%zu}",
             db_ok ? "ok" : "degraded",
             db_ok ? "up" : "down",
             metrics.queue_depth);

    if (!db_ok) {
        set_last_error(error);
    }

    pthread_mutex_unlock(&g_runtime.lock);
    return g_runtime.json_health;
}

const char *engine_last_error(void) {
    return g_runtime.last_error;
}
