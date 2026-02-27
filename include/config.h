#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

#include "logger.h"

typedef struct {
    char db_host[128];
    int db_port;
    char db_name[128];
    char db_user[128];
    char db_password[128];
    int db_connect_timeout;
    size_t buffer_capacity;
    size_t auto_process_threshold;
    size_t process_batch_size;
    size_t pending_preview_limit;
    LoggerLevel log_level;
    int api_port;
} AppConfig;

int config_load_from_env(AppConfig *config, char *error, size_t error_size);
int config_build_conninfo(const AppConfig *config, char *buffer, size_t buffer_size);

#endif
