#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>
#include <stdio.h>

typedef enum {
    LOGGER_DEBUG = 0,
    LOGGER_INFO = 1,
    LOGGER_ERROR = 2
} LoggerLevel;

typedef struct {
    LoggerLevel min_level;
    FILE *stream;
    pthread_mutex_t mutex;
    int initialized;
} AppLogger;

LoggerLevel logger_level_from_string(const char *text);
const char *logger_level_to_string(LoggerLevel level);
int logger_init(AppLogger *logger, LoggerLevel min_level, FILE *stream);
void logger_close(AppLogger *logger);
void logger_log(AppLogger *logger, LoggerLevel level, const char *component, const char *fmt, ...);

#endif
