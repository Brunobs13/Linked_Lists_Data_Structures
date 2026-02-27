#ifndef ENGINE_API_H
#define ENGINE_API_H

#include <stddef.h>

int engine_init(void);
int engine_shutdown(void);
int engine_add_log(const char *level, const char *message, const char *source);
const char *engine_get_pending_logs(void);
const char *engine_process_queue(size_t max_items);
const char *engine_get_metrics(void);
const char *engine_health(void);
const char *engine_last_error(void);

#endif
