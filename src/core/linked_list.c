#include "linked_list.h"

#include <stdlib.h>

void linked_list_init(LinkedList *list) {
    if (list == NULL) {
        return;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

static int linked_list_attach_after(LinkedList *list, LinkedListNode *node, LinkedListNode *after) {
    if (list == NULL || node == NULL) {
        return 0;
    }

    if (after == NULL) {
        node->prev = NULL;
        node->next = list->head;
        if (list->head != NULL) {
            list->head->prev = node;
        }
        list->head = node;
        if (list->tail == NULL) {
            list->tail = node;
        }
        list->size++;
        return 1;
    }

    node->prev = after;
    node->next = after->next;
    after->next = node;

    if (node->next != NULL) {
        node->next->prev = node;
    } else {
        list->tail = node;
    }

    list->size++;
    return 1;
}

int linked_list_push_back(LinkedList *list, LogEntry *entry) {
    if (list == NULL || entry == NULL) {
        return 0;
    }

    LinkedListNode *node = (LinkedListNode *)calloc(1, sizeof(LinkedListNode));
    if (node == NULL) {
        return 0;
    }

    node->entry = entry;
    return linked_list_attach_after(list, node, list->tail);
}

int linked_list_push_front(LinkedList *list, LogEntry *entry) {
    if (list == NULL || entry == NULL) {
        return 0;
    }

    LinkedListNode *node = (LinkedListNode *)calloc(1, sizeof(LinkedListNode));
    if (node == NULL) {
        return 0;
    }

    node->entry = entry;
    return linked_list_attach_after(list, node, NULL);
}

LogEntry *linked_list_pop_front(LinkedList *list) {
    if (list == NULL || list->head == NULL) {
        return NULL;
    }

    LinkedListNode *node = list->head;
    LogEntry *entry = node->entry;

    list->head = node->next;
    if (list->head != NULL) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;
    }

    if (list->size > 0) {
        list->size--;
    }

    free(node);
    return entry;
}

size_t linked_list_size(const LinkedList *list) {
    if (list == NULL) {
        return 0;
    }

    return list->size;
}

void linked_list_clear(LinkedList *list, void (*entry_free_fn)(LogEntry *)) {
    if (list == NULL) {
        return;
    }

    LinkedListNode *cursor = list->head;
    while (cursor != NULL) {
        LinkedListNode *next = cursor->next;
        if (entry_free_fn != NULL && cursor->entry != NULL) {
            entry_free_fn(cursor->entry);
        }
        free(cursor);
        cursor = next;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}
