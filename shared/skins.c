#include "c4p.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

int c4p_color_parse_hex(const char *hex, c4p_color *out) {
    if (!hex || !out) return 0;
    while (*hex == ' ' || *hex == '\t' || *hex == '\n' || *hex == '\r') hex++;
    if (*hex == '#') hex++;
    size_t length = strlen(hex);
    while (length > 0) {
        char last = hex[length - 1];
        if (last == ' ' || last == '\t' || last == '\n' || last == '\r') length--;
        else break;
    }
    if (length != 6 && length != 8) return 0;
    unsigned long packed = 0;
    size_t i;
    for (i = 0; i < length; i++) {
        char c = hex[i];
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return 0;
        packed = (packed << 4) | (unsigned long)digit;
    }
    if (length == 6) {
        out->r = (double)((packed >> 16) & 0xFF) / 255.0;
        out->g = (double)((packed >> 8) & 0xFF) / 255.0;
        out->b = (double)(packed & 0xFF) / 255.0;
        out->a = 1.0;
    } else {
        out->r = (double)((packed >> 24) & 0xFF) / 255.0;
        out->g = (double)((packed >> 16) & 0xFF) / 255.0;
        out->b = (double)((packed >> 8) & 0xFF) / 255.0;
        out->a = (double)(packed & 0xFF) / 255.0;
    }
    return 1;
}

/* --- built-in skins ---------------------------------------------------- */

/* Values copied from SkinTheme.builtins in macos/Core/SkinTheme.swift so the
 * three platforms show the same colours. */
static const c4p_skin builtin_skins[] = {
    { "dark", "Dark", "",
      { 0.15, 0.15, 0.17, 1.0 }, { 1.00, 1.00, 1.00, 0.18 }, { 0.96, 0.96, 0.96, 1.0 },
      { 0.67, 0.67, 0.67, 1.0 }, { 0.86, 0.86, 0.86, 1.0 }, 0, 9.0, "", 0 },
    { "light", "Light", "",
      { 0.96, 0.96, 0.97, 1.0 }, { 0.55, 0.65, 0.82, 0.55 }, { 0.10, 0.10, 0.10, 1.0 },
      { 0.42, 0.42, 0.42, 1.0 }, { 0.35, 0.35, 0.35, 1.0 }, 1, 9.0, "", 0 },
    { "cyan", "Cyan", "",
      { 0.03, 0.14, 0.18, 1.0 }, { 0.05, 0.45, 0.55, 0.85 }, { 0.96, 0.96, 0.96, 1.0 },
      { 0.55, 0.75, 0.80, 1.0 }, { 0.70, 0.95, 1.00, 1.0 }, 0, 9.0, "", 0 },
    { "midnight", "Midnight", "",
      { 0.05, 0.07, 0.16, 1.0 }, { 0.18, 0.25, 0.55, 0.85 }, { 0.96, 0.96, 0.96, 1.0 },
      { 0.58, 0.63, 0.78, 1.0 }, { 0.78, 0.84, 1.00, 1.0 }, 0, 9.0, "", 0 },
};

int c4p_skin_builtin_count(void) { return ARRAY_COUNT(builtin_skins); }

const c4p_skin *c4p_skin_builtin_at(int index) {
    if (index < 0 || index >= c4p_skin_builtin_count()) return NULL;
    return &builtin_skins[index];
}

const c4p_skin *c4p_skin_builtin_find(const char *id) {
    if (!id) return NULL;
    int i;
    for (i = 0; i < c4p_skin_builtin_count(); i++) {
        if (strcmp(builtin_skins[i].id, id) == 0) return &builtin_skins[i];
    }
    return NULL;
}

const c4p_skin *c4p_skin_default(void) { return &builtin_skins[0]; }

/* --- parsing ---------------------------------------------------------- */

static void set_error(char *error, size_t length, const char *message) {
    if (error && length) {
        snprintf(error, length, "%s", message);
    }
}

static void copy_field(char *destination, size_t length, const char *source) {
    if (!destination || !length) return;
    snprintf(destination, length, "%s", source ? source : "");
}

static int color_from(json_value *colors, const char *key, c4p_color *out,
                      char *error, size_t error_length) {
    json_value *raw = json_object_get(colors, key);
    if (!raw) {
        char message[160];
        snprintf(message, sizeof message, "The skin is missing the color \"%s\".", key);
        set_error(error, error_length, message);
        return 0;
    }
    if (raw->type != JSON_STRING || !c4p_color_parse_hex(raw->string, out)) {
        char message[180];
        snprintf(message, sizeof message,
                 "Color \"%s\" is not a valid #RRGGBB or #RRGGBBAA value.", key);
        set_error(error, error_length, message);
        return 0;
    }
    return 1;
}

int c4p_skin_from_json(const char *json, size_t length, const char *id, c4p_skin *out,
                       char *error, size_t error_length) {
    if (!json || !out) return 0;
    json_value *root = json_parse(json, length);
    if (!root || root->type != JSON_OBJECT) {
        json_free(root);
        set_error(error, error_length, "The file must be a JSON object.");
        return 0;
    }

    memset(out, 0, sizeof *out);
    copy_field(out->id, sizeof out->id, id);

    json_value *name = json_object_get(root, "name");
    if (!name || name->type != JSON_STRING || name->string[0] == '\0') {
        json_free(root);
        set_error(error, error_length, "The skin is missing a \"name\".");
        return 0;
    }
    copy_field(out->name, sizeof out->name, name->string);

    json_value *author = json_object_get(root, "author");
    if (author && author->type == JSON_STRING) {
        copy_field(out->author, sizeof out->author, author->string);
    }

    json_value *colors = json_object_get(root, "colors");
    if (!colors || colors->type != JSON_OBJECT) {
        json_free(root);
        set_error(error, error_length, "The skin is missing its \"colors\" object.");
        return 0;
    }
    if (!color_from(colors, "background", &out->background, error, error_length) ||
        !color_from(colors, "selection", &out->selection, error, error_length) ||
        !color_from(colors, "foreground", &out->foreground, error, error_length) ||
        !color_from(colors, "muted", &out->muted, error, error_length) ||
        !color_from(colors, "delete", &out->delete_color, error, error_length)) {
        json_free(root);
        return 0;
    }

    json_value *appearance = json_object_get(root, "appearance");
    if (appearance) {
        if (appearance->type != JSON_STRING) {
            json_free(root);
            set_error(error, error_length, "\"appearance\" must be \"dark\" or \"light\" (got \"\").");
            return 0;
        }
        if (strcmp(appearance->string, "light") == 0) {
            out->light = 1;
        } else if (strcmp(appearance->string, "dark") == 0) {
            out->light = 0;
        } else {
            char message[200];
            snprintf(message, sizeof message,
                     "\"appearance\" must be \"dark\" or \"light\" (got \"%s\").", appearance->string);
            json_free(root);
            set_error(error, error_length, message);
            return 0;
        }
    }

    json_value *radius = json_object_get(root, "cornerRadius");
    if (radius) {
        if (radius->type != JSON_NUMBER || radius->number < 0 || radius->number > 30) {
            json_free(root);
            set_error(error, error_length, "\"cornerRadius\" must be a number between 0 and 30.");
            return 0;
        }
        out->corner_radius = radius->number;
    } else {
        out->corner_radius = 9.0;
    }

    json_value *font = json_object_get(root, "font");
    if (font) {
        if (font->type != JSON_OBJECT) {
            json_free(root);
            set_error(error, error_length,
                      "\"font\" must be an object with an optional \"name\" and \"size\".");
            return 0;
        }
        json_value *font_name = json_object_get(font, "name");
        if (font_name && font_name->type == JSON_STRING && font_name->string[0] != '\0') {
            copy_field(out->font_name, sizeof out->font_name, font_name->string);
        }
        json_value *font_size = json_object_get(font, "size");
        if (font_size && font_size->type == JSON_NUMBER && font_size->number >= 8 &&
            font_size->number <= 36) {
            out->font_size = font_size->number;
        }
    }

    json_free(root);
    return 1;
}

/* --- ids -------------------------------------------------------------- */

void c4p_skin_safe_id(const char *file_name, char *out, size_t out_length) {
    if (!out || !out_length) return;
    out[0] = '\0';
    if (!file_name) {
        snprintf(out, out_length, "skin");
        return;
    }
    size_t length = strlen(file_name);
    if (length >= 5) {
        const char *tail = file_name + length - 5;
        char lowered[6];
        int i;
        for (i = 0; i < 5; i++) {
            char c = tail[i];
            lowered[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        }
        lowered[5] = '\0';
        if (strcmp(lowered, ".json") == 0) length -= 5;
    }
    size_t written = 0;
    size_t i;
    for (i = 0; i < length && written + 1 < out_length; i++) {
        unsigned char c = (unsigned char)file_name[i];
        int allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '-' || c == '_' || c == ' ';
        if (allowed) out[written++] = (char)c;
    }
    out[written] = '\0';
    while (written > 0 && out[written - 1] == ' ') out[--written] = '\0';
    size_t start = 0;
    while (out[start] == ' ') start++;
    if (start > 0) memmove(out, out + start, strlen(out + start) + 1);
    if (out[0] == '\0') snprintf(out, out_length, "skin");
}

/* --- file system ------------------------------------------------------ */

void c4p_mkdir_p(const char *path) {
    if (!path || !*path) return;
    char buffer[4096];
    snprintf(buffer, sizeof buffer, "%s", path);
#ifdef _WIN32
    size_t i;
    for (i = 0; buffer[i]; i++) {
        if ((buffer[i] == '/' || buffer[i] == '\\') && i > 2) {
            char saved = buffer[i];
            buffer[i] = '\0';
            _mkdir(buffer);
            buffer[i] = saved;
        }
    }
    _mkdir(buffer);
#else
    size_t i;
    for (i = 1; buffer[i]; i++) {
        if (buffer[i] == '/') {
            buffer[i] = '\0';
            mkdir(buffer, 0755);
            buffer[i] = '/';
        }
    }
    mkdir(buffer, 0755);
#endif
}

char *c4p_read_file(const char *path, size_t *length_out) {
    if (length_out) *length_out = 0;
    if (!path) return NULL;
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    char *data = (char *)malloc((size_t)size + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }
    size_t read = fread(data, 1, (size_t)size, file);
    fclose(file);
    data[read] = '\0';
    if (length_out) *length_out = read;
    return data;
}

int c4p_write_file_atomic(const char *path, const char *data, size_t length) {
    char temporary[4096];
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    FILE *file = fopen(temporary, "wb");
    if (!file) return 0;
    size_t written = fwrite(data, 1, length, file);
    if (fclose(file) != 0 || written != length) {
        remove(temporary);
        return 0;
    }
    remove(path);
    if (rename(temporary, path) != 0) {
        remove(temporary);
        return 0;
    }
    return 1;
}
