#include <assert.h>
#include <stdlib.h>

#include "linked_list.h"
#include "log_entry.h"

static LogEntry *new_entry(uint64_t id) {
    LogEntry *entry = (LogEntry *)calloc(1, sizeof(LogEntry));
    assert(entry != NULL);
    assert(log_entry_init(entry, id, "INFO", "test", "payload", log_entry_now_ms()));
    return entry;
}

int main(void) {
    LinkedList list;
    linked_list_init(&list);

    assert(linked_list_size(&list) == 0);

    LogEntry *a = new_entry(1);
    LogEntry *b = new_entry(2);
    LogEntry *c = new_entry(3);

    assert(linked_list_push_back(&list, a));
    assert(linked_list_push_back(&list, b));
    assert(linked_list_push_front(&list, c));

    assert(linked_list_size(&list) == 3);

    LogEntry *first = linked_list_pop_front(&list);
    assert(first != NULL);
    assert(first->id == 3);
    free(first);

    LogEntry *second = linked_list_pop_front(&list);
    assert(second != NULL);
    assert(second->id == 1);
    free(second);

    LogEntry *third = linked_list_pop_front(&list);
    assert(third != NULL);
    assert(third->id == 2);
    free(third);

    assert(linked_list_pop_front(&list) == NULL);
    assert(linked_list_size(&list) == 0);
    return 0;
}
