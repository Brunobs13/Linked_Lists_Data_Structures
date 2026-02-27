#ifndef BUFFER_ENGINE_H
#define BUFFER_ENGINE_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "linked_list.h"
#include "logger.h"

typedef struct {
    uint64_t total_ingested;
    uint64_t total_processed;
    uint64_t total_errors;
    size_t queue_depth;
    size_t buffer_capacity;
    size_t memory_bytes_estimate;
    double last_processing_ms;
    int64_t started_at_ms;
    int64_t last_processed_at_ms;
} EngineMetrics;

typedef struct {
    LinkedList queue;
    pthread_mutex_t mutex;
    uint64_t next_log_id;
    size_t capacity;
    EngineMetrics metrics;
    AppLogger *logger;
    int initialized;
} BufferEngine;

int buffer_engine_init(BufferEngine *engine, size_t capacity, AppLogger *logger, char *error, size_t error_size);
void buffer_engine_shutdown(BufferEngine *engine);
int buffer_engine_enqueue(BufferEngine *engine,
                          const char *level,
                          const char *source,
                          const char *message,
                          char *error,
                          size_t error_size);
int buffer_engine_requeue_front(BufferEngine *engine, LogEntry *entry, char *error, size_t error_size);
int buffer_engine_dequeue(BufferEngine *engine, LogEntry **entry_out);
int buffer_engine_get_metrics(BufferEngine *engine, EngineMetrics *out_metrics);
void buffer_engine_mark_processed(BufferEngine *engine, double processing_ms);
void buffer_engine_mark_error(BufferEngine *engine);
int buffer_engine_pending_json(BufferEngine *engine,
                               size_t max_items,
                               char *buffer,
                               size_t buffer_size);

#endif
