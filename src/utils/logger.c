#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void timestamp_utc(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm tm_data;

#if defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    gmtime_r(&now, &tm_data);
#else
    struct tm *tmp = gmtime(&now);
    if (tmp != NULL) {
        tm_data = *tmp;
    } else {
        memset(&tm_data, 0, sizeof(tm_data));
    }
#endif

    strftime(buffer, size, "%Y-%m-%dT%H:%M:%SZ", &tm_data);
}

LoggerLevel logger_level_from_string(const char *text) {
    if (text == NULL) {
        return LOGGER_INFO;
    }

    if (strcmp(text, "DEBUG") == 0) {
        return LOGGER_DEBUG;
    }
    if (strcmp(text, "ERROR") == 0) {
        return LOGGER_ERROR;
    }

    return LOGGER_INFO;
}

const char *logger_level_to_string(LoggerLevel level) {
    switch (level) {
        case LOGGER_DEBUG:
            return "DEBUG";
        case LOGGER_ERROR:
            return "ERROR";
        case LOGGER_INFO:
        default:
            return "INFO";
    }
}

int logger_init(AppLogger *logger, LoggerLevel min_level, FILE *stream) {
    if (logger == NULL) {
        return 0;
    }

    logger->min_level = min_level;
    logger->stream = stream != NULL ? stream : stdout;
    logger->initialized = 0;

    if (pthread_mutex_init(&logger->mutex, NULL) != 0) {
        return 0;
    }

    logger->initialized = 1;
    return 1;
}

void logger_close(AppLogger *logger) {
    if (logger == NULL || !logger->initialized) {
        return;
    }

    pthread_mutex_destroy(&logger->mutex);
    logger->initialized = 0;
}

void logger_log(AppLogger *logger, LoggerLevel level, const char *component, const char *fmt, ...) {
    if (logger == NULL || !logger->initialized) {
        return;
    }

    if (level < logger->min_level) {
        return;
    }

    char timestamp[32] = {0};
    timestamp_utc(timestamp, sizeof(timestamp));

    va_list args;
    va_start(args, fmt);
    char message[1024] = {0};
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    pthread_mutex_lock(&logger->mutex);
    fprintf(logger->stream,
            "{\"ts\":\"%s\",\"level\":\"%s\",\"component\":\"%s\",\"msg\":\"",
            timestamp,
            logger_level_to_string(level),
            component != NULL ? component : "unknown");

    for (size_t i = 0; message[i] != '\0'; ++i) {
        switch (message[i]) {
            case '\\':
                fputs("\\\\", logger->stream);
                break;
            case '"':
                fputs("\\\"", logger->stream);
                break;
            case '\n':
                fputs("\\n", logger->stream);
                break;
            case '\r':
                fputs("\\r", logger->stream);
                break;
            case '\t':
                fputs("\\t", logger->stream);
                break;
            default:
                fputc(message[i], logger->stream);
                break;
        }
    }

    fprintf(logger->stream, "\"}\n");
    fflush(logger->stream);
    pthread_mutex_unlock(&logger->mutex);
}
