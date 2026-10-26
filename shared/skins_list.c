#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "c4p.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

void c4p_skin_list_init(c4p_skin_list *list) {
    if (!list) return;
    list->items = NULL;
    list->count = 0;
}

void c4p_skin_list_free(c4p_skin_list *list) {
    if (!list) return;
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

#ifndef _WIN32
static int has_json_suffix(const char *name) {
    size_t length = name ? strlen(name) : 0;
    if (length < 6) return 0;
    const char *tail = name + length - 5;
    return tail[0] == '.' && (tail[1] == 'j' || tail[1] == 'J') && (tail[2] == 's' || tail[2] == 'S') &&
           (tail[3] == 'o' || tail[3] == 'O') && (tail[4] == 'n' || tail[4] == 'N');
}
#endif

static int append_installed(c4p_skin **items, int count, const char *directory, const char *file_name) {
    char path[4096];
#ifdef _WIN32
    snprintf(path, sizeof path, "%s\\%s", directory, file_name);
#else
    snprintf(path, sizeof path, "%s/%s", directory, file_name);
#endif
    size_t length = 0;
    char *data = c4p_read_file(path, &length);
    if (!data) return count;
    c4p_skin skin;
    char identifier[96];
    c4p_skin_safe_id(file_name, identifier, sizeof identifier);
    char error[256];
    int ok = c4p_skin_from_json(data, length, identifier, &skin, error, sizeof error);
    free(data);
    if (!ok) return count;
    c4p_skin *bigger = (c4p_skin *)realloc(*items, sizeof(c4p_skin) * (size_t)(count + 1));
    if (!bigger) return count;
    *items = bigger;
    (*items)[count] = skin;
    return count + 1;
}

static int compare_skins_by_name(const void *left, const void *right) {
    const c4p_skin *a = (const c4p_skin *)left;
    const c4p_skin *b = (const c4p_skin *)right;
    const char *pa = a->name;
    const char *pb = b->name;
    while (*pa && *pb) {
        unsigned char ca = (unsigned char)*pa;
        unsigned char cb = (unsigned char)*pb;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb) return (ca < cb) ? -1 : 1;
        pa++;
        pb++;
    }
    if (*pa == '\0' && *pb != '\0') return -1;
    if (*pa != '\0' && *pb == '\0') return 1;
    return 0;
}

int c4p_skin_list_load(c4p_skin_list *list, const char *directory) {
    if (!list) return 0;
    c4p_skin_list_free(list);
    int builtins = c4p_skin_builtin_count();
    c4p_skin *items = (c4p_skin *)calloc((size_t)builtins, sizeof(c4p_skin));
    if (!items) return 0;
    int i;
    for (i = 0; i < builtins; i++) {
        items[i] = *c4p_skin_builtin_at(i);
    }
    int count = builtins;

    if (directory && *directory) {
#ifdef _WIN32
        char pattern[4096];
        snprintf(pattern, sizeof pattern, "%s\\*.json", directory);
        WIN32_FIND_DATAA entry;
        HANDLE found = FindFirstFileA(pattern, &entry);
        if (found != INVALID_HANDLE_VALUE) {
            do {
                if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    count = append_installed(&items, count, directory, entry.cFileName);
                }
            } while (FindNextFileA(found, &entry));
            FindClose(found);
        }
#else
        DIR *dir = opendir(directory);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                if (!has_json_suffix(entry->d_name)) continue;
                count = append_installed(&items, count, directory, entry->d_name);
            }
            closedir(dir);
        }
#endif
        if (count > builtins) {
            qsort(items + builtins, (size_t)(count - builtins), sizeof(c4p_skin), compare_skins_by_name);
        }
    }

    list->items = items;
    list->count = count;
    return 1;
}

const c4p_skin *c4p_skin_list_find(const c4p_skin_list *list, const char *id) {
    if (!list || !id) return NULL;
    int i;
    for (i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].id, id) == 0) return &list->items[i];
    }
    return NULL;
}

const c4p_skin *c4p_skin_list_resolve(const c4p_skin_list *list, const char *id) {
    const c4p_skin *found = c4p_skin_list_find(list, id);
    if (found) return found;
    const c4p_skin *fallback = c4p_skin_list_find(list, c4p_skin_default()->id);
    if (fallback) return fallback;
    return c4p_skin_default();
}

int c4p_skin_import(const char *directory, const char *source_path, char *id_out, size_t id_length,
                    char *error, size_t error_length) {
    if (error && error_length) error[0] = '\0';
    size_t length = 0;
    char *data = c4p_read_file(source_path, &length);
    if (!data) {
        if (error && error_length) snprintf(error, error_length, "The selected file could not be read.");
        return 0;
    }

    const char *file_name = source_path;
    const char *slash = strrchr(source_path, '/');
    const char *backslash = strrchr(source_path, '\\');
    if (slash) file_name = slash + 1;
    if (backslash && backslash + 1 > file_name) file_name = backslash + 1;

    char identifier[96];
    c4p_skin_safe_id(file_name, identifier, sizeof identifier);
    if (c4p_skin_builtin_find(identifier)) {
        char adjusted[96];
        snprintf(adjusted, sizeof adjusted, "%.86s-custom", identifier);
        snprintf(identifier, sizeof identifier, "%s", adjusted);
    }

    c4p_skin skin;
    if (!c4p_skin_from_json(data, length, identifier, &skin, error, error_length)) {
        free(data);
        return 0;
    }

    c4p_mkdir_p(directory);
    char destination[4096];
#ifdef _WIN32
    snprintf(destination, sizeof destination, "%s\\%s.json", directory, identifier);
#else
    snprintf(destination, sizeof destination, "%s/%s.json", directory, identifier);
#endif
    if (!c4p_write_file_atomic(destination, data, length)) {
        free(data);
        if (error && error_length) snprintf(error, error_length, "The skin could not be saved.");
        return 0;
    }
    free(data);
    if (id_out && id_length) snprintf(id_out, id_length, "%s", identifier);
    return 1;
}
