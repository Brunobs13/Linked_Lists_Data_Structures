#include "queue_processor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log_entry.h"

static void write_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s", message);
    }
}

int queue_processor_init(QueueProcessor *processor,
                         BufferEngine *engine,
                         Persistence *persistence,
                         AppLogger *logger,
                         size_t default_batch_size,
                         char *error,
                         size_t error_size) {
    if (processor == NULL || engine == NULL || persistence == NULL) {
        write_error(error, error_size, "Invalid queue processor arguments.");
        return 0;
    }

    memset(processor, 0, sizeof(*processor));
    processor->engine = engine;
    processor->persistence = persistence;
    processor->logger = logger;
    processor->default_batch_size = default_batch_size > 0 ? default_batch_size : 1;

    logger_log(logger,
               LOGGER_INFO,
               "queue_processor",
               "initialized batch_size=%zu",
               processor->default_batch_size);

    return 1;
}

int queue_processor_process(QueueProcessor *processor,
                            size_t max_items,
                            size_t *processed_count,
                            double *elapsed_ms,
                            char *error,
                            size_t error_size) {
    if (processor == NULL || processor->engine == NULL || processor->persistence == NULL) {
        write_error(error, error_size, "Queue processor is not initialized.");
        return 0;
    }

    size_t limit = max_items > 0 ? max_items : processor->default_batch_size;
    if (limit == 0) {
        limit = 1;
    }

    const int64_t started_at = log_entry_now_ms();
    size_t processed = 0;

    while (processed < limit) {
        LogEntry *entry = NULL;
        if (!buffer_engine_dequeue(processor->engine, &entry)) {
            break;
        }

        int64_t processed_at = log_entry_now_ms();
        double processing_cost = (double)(processed_at - entry->ingested_at_ms);

        if (!persistence_insert_processed_log(processor->persistence,
                                              entry,
                                              processed_at,
                                              processing_cost,
                                              error,
                                              error_size)) {
            buffer_engine_mark_error(processor->engine);

            char requeue_error[256] = {0};
            if (!buffer_engine_requeue_front(processor->engine, entry, requeue_error, sizeof(requeue_error))) {
                logger_log(processor->logger,
                           LOGGER_ERROR,
                           "queue_processor",
                           "failed to requeue log_id=%llu reason=%s",
                           (unsigned long long)entry->id,
                           requeue_error);
                free(entry);
            }

            return 0;
        }

        free(entry);
        buffer_engine_mark_processed(processor->engine, processing_cost);
        processed++;
    }

    const int64_t finished_at = log_entry_now_ms();
    double elapsed = (double)(finished_at - started_at);

    if (processed_count != NULL) {
        *processed_count = processed;
    }

    if (elapsed_ms != NULL) {
        *elapsed_ms = elapsed;
    }

    EngineMetrics metrics;
    if (processed > 0 && buffer_engine_get_metrics(processor->engine, &metrics)) {
        char metric_error[256] = {0};
        if (!persistence_insert_metrics(processor->persistence, &metrics, metric_error, sizeof(metric_error))) {
            buffer_engine_mark_error(processor->engine);
            logger_log(processor->logger,
                       LOGGER_ERROR,
                       "queue_processor",
                       "failed to persist metrics: %s",
                       metric_error);
        }
    }

    return 1;
}
