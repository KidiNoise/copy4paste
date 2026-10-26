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
#include <unistd.h>
#endif

char *c4p_config_dir(void) {
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    char buffer[4096];
    if (appdata && *appdata) {
        snprintf(buffer, sizeof buffer, "%s", appdata);
    } else {
        const char *profile = getenv("USERPROFILE");
        snprintf(buffer, sizeof buffer, "%s\\AppData\\Roaming", profile ? profile : ".");
    }
    char *joined = (char *)malloc(strlen(buffer) + 16);
    if (!joined) return NULL;
    sprintf(joined, "%s\\copy4paste", buffer);
    return joined;
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char buffer[4096];
    if (xdg && *xdg) {
        snprintf(buffer, sizeof buffer, "%s", xdg);
    } else {
        const char *home = getenv("HOME");
        snprintf(buffer, sizeof buffer, "%s/.config", home ? home : ".");
    }
    char *joined = (char *)malloc(strlen(buffer) + 16);
    if (!joined) return NULL;
    sprintf(joined, "%s/copy4paste", buffer);
    return joined;
#endif
}

char *c4p_skins_dir(void) {
    char *dir = c4p_config_dir();
    if (!dir) return NULL;
    size_t length = strlen(dir);
    char *joined = (char *)malloc(length + 8);
    if (!joined) {
        free(dir);
        return NULL;
    }
#ifdef _WIN32
    sprintf(joined, "%s\\skins", dir);
#else
    sprintf(joined, "%s/skins", dir);
#endif
    free(dir);
    return joined;
}

char *c4p_config_path(void) {
    char *dir = c4p_config_dir();
    if (!dir) return NULL;
    size_t length = strlen(dir);
    char *joined = (char *)malloc(length + 16);
    if (!joined) {
        free(dir);
        return NULL;
    }
#ifdef _WIN32
    sprintf(joined, "%s\\settings.ini", dir);
#else
    sprintf(joined, "%s/settings.ini", dir);
#endif
    free(dir);
    return joined;
}

void c4p_config_defaults(c4p_config *config) {
    if (!config) return;
    config->capacity = C4P_CAPACITY_DEFAULT;
    snprintf(config->skin, sizeof config->skin, "%s", c4p_skin_default()->id);
    config->opacity = C4P_OPACITY_DEFAULT;
}

static void sanitize(c4p_config *config) {
    if (config->capacity < C4P_CAPACITY_MIN) config->capacity = C4P_CAPACITY_MIN;
    if (config->capacity > C4P_CAPACITY_MAX) config->capacity = C4P_CAPACITY_MAX;
    if (config->opacity < C4P_OPACITY_MIN) config->opacity = C4P_OPACITY_MIN;
    if (config->opacity > C4P_OPACITY_MAX) config->opacity = C4P_OPACITY_MAX;
    if (config->skin[0] == '\0') snprintf(config->skin, sizeof config->skin, "%s", c4p_skin_default()->id);
}

int c4p_config_load(c4p_config *config, const char *path) {
    if (!config) return 0;
    c4p_config_defaults(config);
    if (!path || !*path) return 0;
    size_t length = 0;
    char *data = c4p_read_file(path, &length);
    if (!data) return 0;

    char *line = data;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            next++;
        }
        size_t line_length = strlen(line);
        while (line_length > 0 && (line[line_length - 1] == '\r' || line[line_length - 1] == ' ')) {
            line[--line_length] = '\0';
        }
        char *equals = strchr(line, '=');
        if (equals) {
            *equals = '\0';
            char *key = line;
            char *value = equals + 1;
            while (*key == ' ' || *key == '\t') key++;
            while (*value == ' ' || *value == '\t') value++;
            if (strcmp(key, "capacity") == 0) {
                config->capacity = atoi(value);
            } else if (strcmp(key, "skin") == 0) {
                snprintf(config->skin, sizeof config->skin, "%s", value);
            } else if (strcmp(key, "opacity") == 0) {
                config->opacity = atoi(value);
            }
        }
        line = next;
    }
    free(data);
    sanitize(config);
    return 1;
}

int c4p_config_save(const c4p_config *config, const char *path) {
    if (!config || !path || !*path) return 0;
    c4p_config copy = *config;
    sanitize(&copy);
    char *dir = c4p_config_dir();
    if (dir) {
        c4p_mkdir_p(dir);
        free(dir);
    }
    char text[512];
    int written = snprintf(text, sizeof text, "capacity=%d\nskin=%s\nopacity=%d\n", copy.capacity,
                           copy.skin, copy.opacity);
    if (written <= 0) return 0;
    return c4p_write_file_atomic(path, text, (size_t)written);
}
