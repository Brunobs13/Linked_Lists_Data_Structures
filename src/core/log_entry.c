#include "log_entry.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

int64_t log_entry_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return 0;
    }

    return ((int64_t)ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

static int copy_text(char *dest, size_t dest_size, const char *value) {
    if (dest == NULL || dest_size == 0 || value == NULL) {
        return 0;
    }

    size_t len = strnlen(value, dest_size);
    if (len == dest_size) {
        return 0;
    }

    snprintf(dest, dest_size, "%s", value);
    return 1;
}

int log_entry_init(LogEntry *entry,
                   uint64_t id,
                   const char *level,
                   const char *source,
                   const char *message,
                   int64_t ingested_at_ms) {
    if (entry == NULL || level == NULL || source == NULL || message == NULL) {
        return 0;
    }

    if (!copy_text(entry->level, sizeof(entry->level), level)) {
        return 0;
    }

    if (!copy_text(entry->source, sizeof(entry->source), source)) {
        return 0;
    }

    if (!copy_text(entry->message, sizeof(entry->message), message)) {
        return 0;
    }

    entry->id = id;
    entry->ingested_at_ms = ingested_at_ms > 0 ? ingested_at_ms : log_entry_now_ms();
    return 1;
}
