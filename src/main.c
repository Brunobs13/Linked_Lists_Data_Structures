#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine_api.h"

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int signal_value) {
    (void)signal_value;
    g_running = 0;
}

static char *trim(char *text) {
    if (text == NULL) {
        return text;
    }

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[len - 1] = '\0';
        len--;
    }

    return text;
}

int main(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (!engine_init()) {
        fprintf(stderr, "engine_init failed: %s\n", engine_last_error());
        return 1;
    }

    printf("Log Engine CLI started.\n");
    printf("Commands:\n");
    printf("  LEVEL|SOURCE|MESSAGE  -> enqueue log\n");
    printf("  process               -> process queue batch\n");
    printf("  metrics               -> show metrics\n");
    printf("  pending               -> show pending queue\n");
    printf("  quit                  -> shutdown\n\n");

    char line[2048];
    while (g_running && fgets(line, sizeof(line), stdin) != NULL) {
        char *input = trim(line);
        if (input[0] == '\0') {
            continue;
        }

        if (strcmp(input, "quit") == 0) {
            break;
        }

        if (strcmp(input, "process") == 0) {
            printf("%s\n", engine_process_queue(0));
            continue;
        }

        if (strcmp(input, "metrics") == 0) {
            printf("%s\n", engine_get_metrics());
            continue;
        }

        if (strcmp(input, "pending") == 0) {
            printf("%s\n", engine_get_pending_logs());
            continue;
        }

        char *level = strtok(input, "|");
        char *source = strtok(NULL, "|");
        char *message = strtok(NULL, "");

        level = trim(level != NULL ? level : "INFO");
        source = trim(source != NULL ? source : "cli");
        message = trim(message != NULL ? message : "");

        if (message[0] == '\0') {
            fprintf(stderr, "Invalid input. Expected LEVEL|SOURCE|MESSAGE\n");
            continue;
        }

        if (!engine_add_log(level, message, source)) {
            fprintf(stderr, "enqueue failed: %s\n", engine_last_error());
        }
    }

    if (!engine_shutdown()) {
        fprintf(stderr, "engine_shutdown failed: %s\n", engine_last_error());
        return 1;
    }

    printf("Shutdown complete.\n");
    return 0;
}
