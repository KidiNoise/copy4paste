#include "c4p.h"

#include <stdlib.h>
#include <string.h>

void c4p_history_init(c4p_history *history, int capacity) {
    if (!history) return;
    history->items = NULL;
    history->count = 0;
    history->capacity = capacity < C4P_CAPACITY_MIN ? C4P_CAPACITY_MIN : capacity;
    if (history->capacity > C4P_CAPACITY_MAX) history->capacity = C4P_CAPACITY_MAX;
    history->max_bytes = C4P_MAX_TEXT_BYTES;
}

void c4p_history_free(c4p_history *history) {
    if (!history) return;
    int i;
    for (i = 0; i < history->count; i++) free(history->items[i]);
    free(history->items);
    history->items = NULL;
    history->count = 0;
}

static void trim(c4p_history *history) {
    while (history->count > history->capacity) {
        free(history->items[history->count - 1]);
        history->count--;
    }
}

void c4p_history_set_capacity(c4p_history *history, int capacity) {
    if (!history) return;
    if (capacity < C4P_CAPACITY_MIN) capacity = C4P_CAPACITY_MIN;
    if (capacity > C4P_CAPACITY_MAX) capacity = C4P_CAPACITY_MAX;
    history->capacity = capacity;
    trim(history);
}

int c4p_history_contains(const c4p_history *history, const char *text) {
    if (!history || !text) return 0;
    int i;
    for (i = 0; i < history->count; i++) {
        if (strcmp(history->items[i], text) == 0) return 1;
    }
    return 0;
}

int c4p_history_remember(c4p_history *history, const char *text) {
    if (!history || !text || !*text) return 0;
    size_t length = strlen(text);
    if (length > history->max_bytes) return 0;

    int i;
    for (i = 0; i < history->count; i++) {
        if (strcmp(history->items[i], text) == 0) {
            free(history->items[i]);
            memmove(history->items + 1, history->items, sizeof(char *) * (size_t)i);
            history->items[0] = (char *)malloc(length + 1);
            if (!history->items[0]) return 0;
            memcpy(history->items[0], text, length + 1);
            return 1;
        }
    }

    char **bigger = (char **)realloc(history->items, sizeof(char *) * (size_t)(history->count + 1));
    if (!bigger) return 0;
    history->items = bigger;
    memmove(history->items + 1, history->items, sizeof(char *) * (size_t)history->count);
    history->items[0] = (char *)malloc(length + 1);
    if (!history->items[0]) return 0;
    memcpy(history->items[0], text, length + 1);
    history->count++;
    trim(history);
    return 1;
}

int c4p_history_remove(c4p_history *history, const char *text) {
    if (!history || !text) return 0;
    int i;
    for (i = 0; i < history->count; i++) {
        if (strcmp(history->items[i], text) == 0) {
            free(history->items[i]);
            memmove(history->items + i, history->items + i + 1,
                    sizeof(char *) * (size_t)(history->count - i - 1));
            history->count--;
            return 1;
        }
    }
    return 0;
}

char *c4p_preview_text(const char *text) {
    if (!text) return NULL;
    size_t length = strlen(text);
    /* Worst case every byte is a newline: each becomes " ↵ " (6 bytes). */
    char *out = (char *)malloc(length * 6 + 1);
    if (!out) return NULL;
    size_t written = 0;
    size_t i;
    for (i = 0; i < length; i++) {
        switch (text[i]) {
            case '\n':
                memcpy(out + written, " \xE2\x86\xB5 ", 5);
                written += 5;
                break;
            case '\r':
                out[written++] = ' ';
                break;
            case '\t':
                memcpy(out + written, " \xE2\x86\xA5 ", 5);
                written += 5;
                break;
            default:
                out[written++] = text[i];
                break;
        }
    }
    out[written] = '\0';
    return out;
}
