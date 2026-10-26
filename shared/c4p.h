/* copy4paste core: clipboard history, skins and preferences.
 *
 * This file is shared by the Windows and Linux builds. It has no UI, no
 * network and no dependencies beyond the C standard library, and it mirrors
 * the rules of the macOS app (macos/Core/SkinTheme.swift and HistoryStore.swift) field by field.
 */
#ifndef C4P_H
#define C4P_H

#include <stddef.h>

#define C4P_VERSION "1.3"

#define C4P_CAPACITY_MIN 1
#define C4P_CAPACITY_MAX 30
#define C4P_CAPACITY_DEFAULT 4
#define C4P_OPACITY_MIN 20
#define C4P_OPACITY_MAX 100
#define C4P_OPACITY_DEFAULT 94
#define C4P_MAX_TEXT_BYTES 2000000

/* Panel geometry, in points. Same numbers as macos/Sources/main.swift. */
#define C4P_PANEL_WIDTH 290
#define C4P_ROW_HEIGHT 31
#define C4P_HEADER_HEIGHT 28
#define C4P_VISIBLE_ROWS 4

typedef struct {
    double r;
    double g;
    double b;
    double a;
} c4p_color;

typedef struct {
    char id[96];
    char name[128];
    char author[128];
    c4p_color background;
    c4p_color selection;
    c4p_color foreground;
    c4p_color muted;
    c4p_color delete_color;
    int light;            /* 1 = light interface, 0 = dark */
    double corner_radius; /* 0..30 */
    char font_name[96];   /* empty = system font */
    double font_size;     /* 0 = default size */
} c4p_skin;

typedef struct {
    c4p_skin *items;
    int count;
} c4p_skin_list;

/* --- colours ---------------------------------------------------------- */

/* Parses #RRGGBB or #RRGGBBAA, with or without the leading '#'. */
int c4p_color_parse_hex(const char *hex, c4p_color *out);

/* --- skins ------------------------------------------------------------ */

/* Parses skin JSON. Returns 1 on success; on failure returns 0 and fills
 * `error` with the same message the macOS app shows. */
int c4p_skin_from_json(const char *json, size_t length, const char *id, c4p_skin *out,
                       char *error, size_t error_length);

int c4p_skin_builtin_count(void);
const c4p_skin *c4p_skin_builtin_at(int index);
const c4p_skin *c4p_skin_builtin_find(const char *id);
const c4p_skin *c4p_skin_default(void);

void c4p_skin_list_init(c4p_skin_list *list);
void c4p_skin_list_free(c4p_skin_list *list);

/* Built-ins first (Dark, Light, Cyan, Midnight), then every readable *.json
 * in `directory`, sorted by name. Unreadable or malformed files are skipped. */
int c4p_skin_list_load(c4p_skin_list *list, const char *directory);

const c4p_skin *c4p_skin_list_find(const c4p_skin_list *list, const char *id);

/* Resolves a stored id, falling back to the default skin (never NULL). */
const c4p_skin *c4p_skin_list_resolve(const c4p_skin_list *list, const char *id);

/* Copies a skin file into `directory`. On success returns 1 and writes the
 * new skin id to `id_out`. On failure returns 0 and fills `error`. */
int c4p_skin_import(const char *directory, const char *source_path, char *id_out,
                    size_t id_length, char *error, size_t error_length);

/* File name (with or without .json) -> safe skin id. */
void c4p_skin_safe_id(const char *file_name, char *out, size_t out_length);

/* --- history (RAM only) ----------------------------------------------- */

typedef struct {
    char **items;
    int count;
    int capacity;
    size_t max_bytes;
} c4p_history;

void c4p_history_init(c4p_history *history, int capacity);
void c4p_history_free(c4p_history *history);
void c4p_history_set_capacity(c4p_history *history, int capacity);

/* Inserts at the top, moves duplicates up, trims to capacity.
 * Returns 1 when the text is now part of the history. */
int c4p_history_remember(c4p_history *history, const char *text);

/* Removes one copy, keeping the order of the rest. */
int c4p_history_remove(c4p_history *history, const char *text);
int c4p_history_contains(const c4p_history *history, const char *text);

/* --- preferences ------------------------------------------------------ */

typedef struct {
    int capacity;
    char skin[96];
    int opacity;
} c4p_config;

void c4p_config_defaults(c4p_config *config);
int c4p_config_load(c4p_config *config, const char *path);
int c4p_config_save(const c4p_config *config, const char *path);

/* Platform directories, malloc'd. Linux: $XDG_CONFIG_HOME/copy4paste
 * (~/.config/copy4paste). Windows: %APPDATA%\copy4paste. */
char *c4p_config_dir(void);
char *c4p_skins_dir(void);
char *c4p_config_path(void);

/* --- helpers ---------------------------------------------------------- */

/* Multi-line and tab-separated text, flattened for the one-line row preview:
 * "\n" -> " ↵ ", "\r" -> " ", "\t" -> " ⇥ ". Caller frees. */
char *c4p_preview_text(const char *text);

void c4p_mkdir_p(const char *path);
char *c4p_read_file(const char *path, size_t *length_out);
int c4p_write_file_atomic(const char *path, const char *data, size_t length);

#endif
