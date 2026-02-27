#ifndef LOG_ENTRY_H
#define LOG_ENTRY_H

#include <stddef.h>
#include <stdint.h>

#define LOG_LEVEL_MAX_LEN 16
#define LOG_SOURCE_MAX_LEN 64
#define LOG_MESSAGE_MAX_LEN 512

typedef struct {
    uint64_t id;
    char level[LOG_LEVEL_MAX_LEN];
    char source[LOG_SOURCE_MAX_LEN];
    char message[LOG_MESSAGE_MAX_LEN];
    int64_t ingested_at_ms;
} LogEntry;

int64_t log_entry_now_ms(void);
int log_entry_init(LogEntry *entry,
                   uint64_t id,
                   const char *level,
                   const char *source,
                   const char *message,
                   int64_t ingested_at_ms);

#endif
