#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *env_or_default(const char *name, const char *fallback) {
    const char *value = getenv(name);
    if (value == NULL || value[0] == '\0') {
        return fallback;
    }
    return value;
}

static int parse_int_env(const char *name, int fallback) {
    const char *value = getenv(name);
    if (value == NULL || value[0] == '\0') {
        return fallback;
    }

    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0') {
        return fallback;
    }

    return (int)parsed;
}

static size_t parse_size_env(const char *name, size_t fallback) {
    const char *value = getenv(name);
    if (value == NULL || value[0] == '\0') {
        return fallback;
    }

    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (end == value || *end != '\0') {
        return fallback;
    }

    return (size_t)parsed;
}

static void write_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s", message);
    }
}

int config_load_from_env(AppConfig *config, char *error, size_t error_size) {
    if (config == NULL) {
        write_error(error, error_size, "Config is NULL.");
        return 0;
    }

    snprintf(config->db_host, sizeof(config->db_host), "%s", env_or_default("DB_HOST", "127.0.0.1"));
    config->db_port = parse_int_env("DB_PORT", 5432);
    snprintf(config->db_name, sizeof(config->db_name), "%s", env_or_default("DB_NAME", "log_engine"));
    snprintf(config->db_user, sizeof(config->db_user), "%s", env_or_default("DB_USER", "log_engine"));
    snprintf(config->db_password, sizeof(config->db_password), "%s", env_or_default("DB_PASSWORD", "log_engine"));
    config->db_connect_timeout = parse_int_env("DB_CONNECT_TIMEOUT", 5);

    config->buffer_capacity = parse_size_env("BUFFER_CAPACITY", 2048);
    config->auto_process_threshold = parse_size_env("AUTO_PROCESS_THRESHOLD", 256);
    config->process_batch_size = parse_size_env("PROCESS_BATCH_SIZE", 200);
    config->pending_preview_limit = parse_size_env("PENDING_PREVIEW_LIMIT", 200);
    config->api_port = parse_int_env("API_PORT", 8000);

    const char *level = env_or_default("LOG_LEVEL", "INFO");
    config->log_level = logger_level_from_string(level);

    if (config->buffer_capacity == 0) {
        write_error(error, error_size, "BUFFER_CAPACITY must be greater than zero.");
        return 0;
    }

    if (config->auto_process_threshold == 0 || config->auto_process_threshold > config->buffer_capacity) {
        config->auto_process_threshold = config->buffer_capacity;
    }

    if (config->process_batch_size == 0) {
        config->process_batch_size = 1;
    }

    if (config->pending_preview_limit == 0) {
        config->pending_preview_limit = 50;
    }

    return 1;
}

int config_build_conninfo(const AppConfig *config, char *buffer, size_t buffer_size) {
    if (config == NULL || buffer == NULL || buffer_size == 0) {
        return 0;
    }

    int written = snprintf(buffer,
                           buffer_size,
                           "host=%s port=%d dbname=%s user=%s password=%s connect_timeout=%d",
                           config->db_host,
                           config->db_port,
                           config->db_name,
                           config->db_user,
                           config->db_password,
                           config->db_connect_timeout);

    return written > 0 && (size_t)written < buffer_size;
}
