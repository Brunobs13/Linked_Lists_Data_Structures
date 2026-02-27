#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <stddef.h>

#include <libpq-fe.h>

#include "buffer_engine.h"
#include "config.h"

typedef struct {
    PGconn *conn;
    AppLogger *logger;
    int initialized;
} Persistence;

int persistence_init(Persistence *persistence, const AppConfig *config, AppLogger *logger, char *error, size_t error_size);
int persistence_ping(Persistence *persistence, char *error, size_t error_size);
int persistence_insert_processed_log(Persistence *persistence,
                                     const LogEntry *entry,
                                     int64_t processed_at_ms,
                                     double processing_ms,
                                     char *error,
                                     size_t error_size);
int persistence_insert_metrics(Persistence *persistence,
                               const EngineMetrics *metrics,
                               char *error,
                               size_t error_size);
void persistence_close(Persistence *persistence);

#endif
