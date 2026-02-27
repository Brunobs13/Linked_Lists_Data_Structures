#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stddef.h>

#include "log_entry.h"

typedef struct LinkedListNode {
    LogEntry *entry;
    struct LinkedListNode *next;
    struct LinkedListNode *prev;
} LinkedListNode;

typedef struct {
    LinkedListNode *head;
    LinkedListNode *tail;
    size_t size;
} LinkedList;

void linked_list_init(LinkedList *list);
int linked_list_push_back(LinkedList *list, LogEntry *entry);
int linked_list_push_front(LinkedList *list, LogEntry *entry);
LogEntry *linked_list_pop_front(LinkedList *list);
size_t linked_list_size(const LinkedList *list);
void linked_list_clear(LinkedList *list, void (*entry_free_fn)(LogEntry *));

#endif
