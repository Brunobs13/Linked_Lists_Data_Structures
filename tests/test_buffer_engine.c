#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer_engine.h"
#include "logger.h"

int main(void) {
    AppLogger logger;
    char error[256] = {0};

    assert(logger_init(&logger, LOGGER_ERROR, stderr));

    BufferEngine engine;
    assert(buffer_engine_init(&engine, 3, &logger, error, sizeof(error)));

    assert(buffer_engine_enqueue(&engine, "INFO", "tests", "one", error, sizeof(error)));
    assert(buffer_engine_enqueue(&engine, "INFO", "tests", "two", error, sizeof(error)));
    assert(buffer_engine_enqueue(&engine, "INFO", "tests", "three", error, sizeof(error)));

    assert(!buffer_engine_enqueue(&engine, "INFO", "tests", "overflow", error, sizeof(error)));
    assert(strstr(error, "capacity") != NULL);

    EngineMetrics metrics;
    assert(buffer_engine_get_metrics(&engine, &metrics));
    assert(metrics.queue_depth == 3);

    char json[2048] = {0};
    assert(buffer_engine_pending_json(&engine, 2, json, sizeof(json)));
    assert(strstr(json, "\"returned\":2") != NULL);

    LogEntry *entry = NULL;
    assert(buffer_engine_dequeue(&engine, &entry));
    assert(entry != NULL);
    free(entry);

    buffer_engine_shutdown(&engine);
    logger_close(&logger);
    return 0;
}
