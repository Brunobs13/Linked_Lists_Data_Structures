#include "buffer_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s", message);
    }
}

static void free_entry(LogEntry *entry) {
    free(entry);
}

static size_t estimate_memory_bytes(size_t queue_depth) {
    return queue_depth * (sizeof(LogEntry) + sizeof(LinkedListNode));
}

int buffer_engine_init(BufferEngine *engine, size_t capacity, AppLogger *logger, char *error, size_t error_size) {
    if (engine == NULL) {
        write_error(error, error_size, "BufferEngine is NULL.");
        return 0;
    }

    if (capacity == 0) {
        write_error(error, error_size, "Buffer capacity must be > 0.");
        return 0;
    }

    memset(engine, 0, sizeof(*engine));
    linked_list_init(&engine->queue);

    if (pthread_mutex_init(&engine->mutex, NULL) != 0) {
        write_error(error, error_size, "Failed to initialize buffer mutex.");
        return 0;
    }

    engine->next_log_id = 1;
    engine->capacity = capacity;
    engine->metrics.buffer_capacity = capacity;
    engine->metrics.started_at_ms = log_entry_now_ms();
    engine->metrics.last_processed_at_ms = 0;
    engine->metrics.last_processing_ms = 0.0;
    engine->metrics.total_errors = 0;
    engine->metrics.total_ingested = 0;
    engine->metrics.total_processed = 0;
    engine->metrics.queue_depth = 0;
    engine->metrics.memory_bytes_estimate = 0;
    engine->logger = logger;
    engine->initialized = 1;

    logger_log(logger, LOGGER_INFO, "buffer_engine", "initialized capacity=%zu", capacity);
    return 1;
}

void buffer_engine_shutdown(BufferEngine *engine) {
    if (engine == NULL || !engine->initialized) {
        return;
    }

    pthread_mutex_lock(&engine->mutex);
    linked_list_clear(&engine->queue, free_entry);
    engine->metrics.queue_depth = 0;
    engine->metrics.memory_bytes_estimate = 0;
    pthread_mutex_unlock(&engine->mutex);

    pthread_mutex_destroy(&engine->mutex);
    engine->initialized = 0;
}

int buffer_engine_enqueue(BufferEngine *engine,
                          const char *level,
                          const char *source,
                          const char *message,
                          char *error,
                          size_t error_size) {
    if (engine == NULL || !engine->initialized) {
        write_error(error, error_size, "Buffer engine is not initialized.");
        return 0;
    }

    if (level == NULL || source == NULL || message == NULL || message[0] == '\0') {
        write_error(error, error_size, "Invalid log payload.");
        return 0;
    }

    pthread_mutex_lock(&engine->mutex);

    if (engine->metrics.queue_depth >= engine->capacity) {
        engine->metrics.total_errors++;
        pthread_mutex_unlock(&engine->mutex);
        write_error(error, error_size, "Buffer capacity reached.");
        return 0;
    }

    LogEntry *entry = (LogEntry *)calloc(1, sizeof(LogEntry));
    if (entry == NULL) {
        engine->metrics.total_errors++;
        pthread_mutex_unlock(&engine->mutex);
        write_error(error, error_size, "Unable to allocate log entry.");
        return 0;
    }

    if (!log_entry_init(entry, engine->next_log_id, level, source, message, log_entry_now_ms())) {
        free(entry);
        engine->metrics.total_errors++;
        pthread_mutex_unlock(&engine->mutex);
        write_error(error, error_size, "Invalid log content lengths.");
        return 0;
    }

    if (!linked_list_push_back(&engine->queue, entry)) {
        free(entry);
        engine->metrics.total_errors++;
        pthread_mutex_unlock(&engine->mutex);
        write_error(error, error_size, "Unable to enqueue entry.");
        return 0;
    }

    engine->next_log_id++;
    engine->metrics.total_ingested++;
    engine->metrics.queue_depth = linked_list_size(&engine->queue);
    engine->metrics.memory_bytes_estimate = estimate_memory_bytes(engine->metrics.queue_depth);

    pthread_mutex_unlock(&engine->mutex);
    return 1;
}

int buffer_engine_requeue_front(BufferEngine *engine, LogEntry *entry, char *error, size_t error_size) {
    if (engine == NULL || !engine->initialized || entry == NULL) {
        write_error(error, error_size, "Invalid requeue request.");
        return 0;
    }

    pthread_mutex_lock(&engine->mutex);

    if (engine->metrics.queue_depth >= engine->capacity) {
        pthread_mutex_unlock(&engine->mutex);
        write_error(error, error_size, "Cannot requeue: capacity reached.");
        return 0;
    }

    if (!linked_list_push_front(&engine->queue, entry)) {
        pthread_mutex_unlock(&engine->mutex);
        write_error(error, error_size, "Cannot requeue: push_front failed.");
        return 0;
    }

    engine->metrics.queue_depth = linked_list_size(&engine->queue);
    engine->metrics.memory_bytes_estimate = estimate_memory_bytes(engine->metrics.queue_depth);
    pthread_mutex_unlock(&engine->mutex);

    return 1;
}

int buffer_engine_dequeue(BufferEngine *engine, LogEntry **entry_out) {
    if (engine == NULL || !engine->initialized || entry_out == NULL) {
        return 0;
    }

    pthread_mutex_lock(&engine->mutex);
    LogEntry *entry = linked_list_pop_front(&engine->queue);
    engine->metrics.queue_depth = linked_list_size(&engine->queue);
    engine->metrics.memory_bytes_estimate = estimate_memory_bytes(engine->metrics.queue_depth);
    pthread_mutex_unlock(&engine->mutex);

    if (entry == NULL) {
        return 0;
    }

    *entry_out = entry;
    return 1;
}

int buffer_engine_get_metrics(BufferEngine *engine, EngineMetrics *out_metrics) {
    if (engine == NULL || !engine->initialized || out_metrics == NULL) {
        return 0;
    }

    pthread_mutex_lock(&engine->mutex);
    *out_metrics = engine->metrics;
    pthread_mutex_unlock(&engine->mutex);
    return 1;
}

void buffer_engine_mark_processed(BufferEngine *engine, double processing_ms) {
    if (engine == NULL || !engine->initialized) {
        return;
    }

    pthread_mutex_lock(&engine->mutex);
    engine->metrics.total_processed++;
    engine->metrics.last_processing_ms = processing_ms;
    engine->metrics.last_processed_at_ms = log_entry_now_ms();
    pthread_mutex_unlock(&engine->mutex);
}

void buffer_engine_mark_error(BufferEngine *engine) {
    if (engine == NULL || !engine->initialized) {
        return;
    }

    pthread_mutex_lock(&engine->mutex);
    engine->metrics.total_errors++;
    pthread_mutex_unlock(&engine->mutex);
}

static int append_raw(char *buffer, size_t buffer_size, size_t *offset, const char *text) {
    if (*offset >= buffer_size) {
        return 0;
    }

    int written = snprintf(buffer + *offset, buffer_size - *offset, "%s", text);
    if (written < 0 || (size_t)written >= (buffer_size - *offset)) {
        return 0;
    }

    *offset += (size_t)written;
    return 1;
}

static int append_escaped(char *buffer, size_t buffer_size, size_t *offset, const char *text) {
    if (!append_raw(buffer, buffer_size, offset, "\"")) {
        return 0;
    }

    for (size_t i = 0; text[i] != '\0'; ++i) {
        char escaped[3] = {0};
        switch (text[i]) {
            case '\\':
                escaped[0] = '\\';
                escaped[1] = '\\';
                if (!append_raw(buffer, buffer_size, offset, escaped)) {
                    return 0;
                }
                break;
            case '"':
                escaped[0] = '\\';
                escaped[1] = '"';
                if (!append_raw(buffer, buffer_size, offset, escaped)) {
                    return 0;
                }
                break;
            case '\n':
                if (!append_raw(buffer, buffer_size, offset, "\\n")) {
                    return 0;
                }
                break;
            case '\r':
                if (!append_raw(buffer, buffer_size, offset, "\\r")) {
                    return 0;
                }
                break;
            case '\t':
                if (!append_raw(buffer, buffer_size, offset, "\\t")) {
                    return 0;
                }
                break;
            default:
                escaped[0] = text[i];
                escaped[1] = '\0';
                if (!append_raw(buffer, buffer_size, offset, escaped)) {
                    return 0;
                }
                break;
        }
    }

    return append_raw(buffer, buffer_size, offset, "\"");
}

int buffer_engine_pending_json(BufferEngine *engine,
                               size_t max_items,
                               char *buffer,
                               size_t buffer_size) {
    if (engine == NULL || !engine->initialized || buffer == NULL || buffer_size == 0) {
        return 0;
    }

    pthread_mutex_lock(&engine->mutex);

    size_t offset = 0;
    char header[128] = {0};
    snprintf(header,
             sizeof(header),
             "{\"queue_depth\":%zu,\"items\":[",
             engine->metrics.queue_depth);

    if (!append_raw(buffer, buffer_size, &offset, header)) {
        pthread_mutex_unlock(&engine->mutex);
        snprintf(buffer, buffer_size, "{\"error\":\"response buffer too small\"}");
        return 0;
    }

    size_t emitted = 0;
    LinkedListNode *node = engine->queue.head;
    while (node != NULL && emitted < max_items) {
        if (emitted > 0 && !append_raw(buffer, buffer_size, &offset, ",")) {
            pthread_mutex_unlock(&engine->mutex);
            snprintf(buffer, buffer_size, "{\"error\":\"response buffer too small\"}");
            return 0;
        }

        char prefix[128] = {0};
        snprintf(prefix,
                 sizeof(prefix),
                 "{\"id\":%llu,\"level\":",
                 (unsigned long long)node->entry->id);

        if (!append_raw(buffer, buffer_size, &offset, prefix) ||
            !append_escaped(buffer, buffer_size, &offset, node->entry->level) ||
            !append_raw(buffer, buffer_size, &offset, ",\"source\":") ||
            !append_escaped(buffer, buffer_size, &offset, node->entry->source) ||
            !append_raw(buffer, buffer_size, &offset, ",\"message\":") ||
            !append_escaped(buffer, buffer_size, &offset, node->entry->message)) {
            pthread_mutex_unlock(&engine->mutex);
            snprintf(buffer, buffer_size, "{\"error\":\"response buffer too small\"}");
            return 0;
        }

        char suffix[128] = {0};
        snprintf(suffix,
                 sizeof(suffix),
                 ",\"ingested_at_ms\":%lld}",
                 (long long)node->entry->ingested_at_ms);

        if (!append_raw(buffer, buffer_size, &offset, suffix)) {
            pthread_mutex_unlock(&engine->mutex);
            snprintf(buffer, buffer_size, "{\"error\":\"response buffer too small\"}");
            return 0;
        }

        emitted++;
        node = node->next;
    }

    char footer[128] = {0};
    snprintf(footer, sizeof(footer), "],\"returned\":%zu}", emitted);

    if (!append_raw(buffer, buffer_size, &offset, footer)) {
        pthread_mutex_unlock(&engine->mutex);
        snprintf(buffer, buffer_size, "{\"error\":\"response buffer too small\"}");
        return 0;
    }

    pthread_mutex_unlock(&engine->mutex);
    return 1;
}
