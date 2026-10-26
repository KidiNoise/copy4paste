/* Minimal JSON reader: just enough for skin files, no dependencies.
 * Only what copy4paste needs is supported: objects, arrays, strings,
 * numbers, booleans and null. Nothing is ever executed. */
#ifndef C4P_JSON_H
#define C4P_JSON_H

#include <stddef.h>

typedef enum { JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT } json_type;

typedef struct json_value json_value;

struct json_value {
    json_type type;
    int boolean;        /* JSON_BOOL */
    double number;      /* JSON_NUMBER */
    char *string;       /* JSON_STRING (UTF-8, NUL terminated) */
    int count;          /* JSON_ARRAY / JSON_OBJECT */
    json_value **items; /* JSON_ARRAY, JSON_OBJECT values */
    char **keys;        /* JSON_OBJECT keys */
};

/* Parses a complete document. Returns NULL on malformed input. */
json_value *json_parse(const char *text, size_t length);
void json_free(json_value *value);

/* Object lookup by exact key. NULL when absent or when `object` is not an object. */
json_value *json_object_get(const json_value *object, const char *key);

#endif
