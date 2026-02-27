#ifndef QUEUE_PROCESSOR_H
#define QUEUE_PROCESSOR_H

#include <stddef.h>

#include "buffer_engine.h"
#include "persistence.h"

typedef struct {
    BufferEngine *engine;
    Persistence *persistence;
    AppLogger *logger;
    size_t default_batch_size;
} QueueProcessor;

int queue_processor_init(QueueProcessor *processor,
                         BufferEngine *engine,
                         Persistence *persistence,
                         AppLogger *logger,
                         size_t default_batch_size,
                         char *error,
                         size_t error_size);
int queue_processor_process(QueueProcessor *processor,
                            size_t max_items,
                            size_t *processed_count,
                            double *elapsed_ms,
                            char *error,
                            size_t error_size);

#endif
