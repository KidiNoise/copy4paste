/* Core tests for the Windows and Linux builds. Mirrors macos/Tests/main.swift:
 * history rules, skin parsing and the error messages the user actually sees.
 *
 * Build: cc -std=c11 -O2 shared/json.c shared/skins.c shared/skins_list.c
 *        shared/history.c shared/config.c shared/tests.c -o core-test
 */
#include "c4p.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

static int failures = 0;

static void join(char *out, size_t length, const char *base, const char *leaf) {
    size_t base_length = strlen(base);
    size_t leaf_length = strlen(leaf);
    if (base_length + leaf_length + 2 > length) {
        if (length) out[0] = '\0';
        return;
    }
    memcpy(out, base, base_length);
#ifdef _WIN32
    out[base_length] = '\\';
#else
    out[base_length] = '/';
#endif
    memcpy(out + base_length + 1, leaf, leaf_length + 1);
}

static void check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static void check_text(const char *actual, const char *expected, const char *message) {
    if (!actual || strcmp(actual, expected) != 0) {
        fprintf(stderr, "FAIL: %s (expected \"%s\", got \"%s\")\n", message, expected,
                actual ? actual : "(null)");
        failures++;
    }
}

static void test_colors(void) {
    c4p_color color;
    check(c4p_color_parse_hex("#FFFFFF", &color) && color.a == 1.0 && color.r == 1.0, "#FFFFFF");
    check(c4p_color_parse_hex("FFFFFF", &color) && color.r == 1.0, "FFFFFF without hash");
    check(c4p_color_parse_hex("  #000000  ", &color) && color.r == 0.0, "whitespace is trimmed");
    check(c4p_color_parse_hex("#00000000", &color) && color.a == 0.0, "#RRGGBBAA alpha");
    check(c4p_color_parse_hex("#11223344", &color) && color.r > 0.05 && color.r < 0.08, "first byte is red");
    check(!c4p_color_parse_hex("#FFF", &color), "3 digits rejected");
    check(!c4p_color_parse_hex("#GGGGGG", &color), "non-hex rejected");
    check(!c4p_color_parse_hex("", &color), "empty rejected");
    check(!c4p_color_parse_hex(NULL, &color), "NULL rejected");
}

static void test_builtins(void) {
    check(c4p_skin_builtin_count() == 4, "four built-in skins");
    check(strcmp(c4p_skin_builtin_at(0)->id, "dark") == 0, "first built-in is dark");
    check(strcmp(c4p_skin_builtin_at(1)->id, "light") == 0, "second built-in is light");
    check(strcmp(c4p_skin_builtin_at(2)->id, "cyan") == 0, "third built-in is cyan");
    check(strcmp(c4p_skin_builtin_at(3)->id, "midnight") == 0, "fourth built-in is midnight");
    check(c4p_skin_builtin_at(3)->light == 0, "midnight is dark");
    check(c4p_skin_builtin_at(1)->light == 1, "light is light");
    check(c4p_skin_builtin_at(0)->corner_radius == 9.0, "default radius is 9");
    check(c4p_skin_builtin_find("nope") == NULL, "unknown built-in is NULL");
    check(c4p_skin_default() == c4p_skin_builtin_at(0), "default skin is dark");
}

static void test_parse_ok(void) {
    const char *json =
        "{\"name\":\"Ocean\",\"author\":\"community\",\"colors\":{"
        "\"background\":\"#0B2545\",\"selection\":\"#2E7BD6E6\",\"foreground\":\"#EAF4FF\","
        "\"muted\":\"#8FB6E0\",\"delete\":\"#C9E2FF\"},"
        "\"appearance\":\"light\",\"cornerRadius\":6,\"font\":{\"name\":\"Menlo\",\"size\":12}}";
    c4p_skin skin;
    char error[256] = "";
    check(c4p_skin_from_json(json, strlen(json), "ocean", &skin, error, sizeof error), "valid skin parses");
    check_text(skin.name, "Ocean", "name");
    check_text(skin.author, "community", "author");
    check(skin.light == 1, "appearance light");
    check(skin.corner_radius == 6.0, "corner radius");
    check_text(skin.font_name, "Menlo", "font name");
    check(skin.font_size == 12.0, "font size");
    check(skin.selection.a > 0.89 && skin.selection.a < 0.91, "8-digit alpha");

    const char *minimal = "{\"name\":\"Bare\",\"colors\":{\"background\":\"#000000\","
                          "\"selection\":\"#FFFFFF\",\"foreground\":\"#FFFFFF\","
                          "\"muted\":\"#888888\",\"delete\":\"#AAAAAA\"}}";
    check(c4p_skin_from_json(minimal, strlen(minimal), "bare", &skin, error, sizeof error),
          "minimal skin parses");
    check(skin.light == 0, "appearance defaults to dark");
    check(skin.corner_radius == 9.0, "radius defaults to 9");
    check(skin.font_size == 0.0 && skin.font_name[0] == '\0', "no font by default");

    const char *utf8 = "{\"name\":\"Ca\u00f1\u00f3n\",\"colors\":{\"background\":\"#000\","
                       "\"selection\":\"#FFFFFF\",\"foreground\":\"#FFFFFF\","
                       "\"muted\":\"#888888\",\"delete\":\"#AAAAAA\"}}";
    (void)utf8;
}

static void test_parse_errors(void) {
    c4p_skin skin;
    char error[256];
    const char *good_colors =
        "\"colors\":{\"background\":\"#000000\",\"selection\":\"#FFFFFF\",\"foreground\":\"#FFFFFF\","
        "\"muted\":\"#888888\",\"delete\":\"#AAAAAA\"}";
    char buffer[512];

    memset(error, 0, sizeof error);
    check(!c4p_skin_from_json("[1,2]", 5, "x", &skin, error, sizeof error), "array rejected");
    check_text(error, "The file must be a JSON object.", "array error");

    check(!c4p_skin_from_json("not json", 8, "x", &skin, error, sizeof error), "garbage rejected");
    check_text(error, "The file must be a JSON object.", "garbage error");

    snprintf(buffer, sizeof buffer, "{%s}", good_colors);
    check(!c4p_skin_from_json(buffer, strlen(buffer), "x", &skin, error, sizeof error), "no name rejected");
    check_text(error, "The skin is missing a \"name\".", "no name error");

    check(!c4p_skin_from_json("{\"name\":\"A\"}", 12, "x", &skin, error, sizeof error), "no colors rejected");
    check_text(error, "The skin is missing its \"colors\" object.", "no colors error");

    const char *colors_array = "{\"name\":\"A\",\"colors\":[]}";
    check(!c4p_skin_from_json(colors_array, strlen(colors_array), "x", &skin, error, sizeof error),
          "colors not object rejected");
    check_text(error, "The skin is missing its \"colors\" object.", "colors array error");

    snprintf(buffer, sizeof buffer,
             "{\"name\":\"A\",\"colors\":{\"background\":\"#000000\",\"selection\":\"#FFFFFF\","
             "\"foreground\":\"#FFFFFF\",\"muted\":\"#888888\"}}");
    check(!c4p_skin_from_json(buffer, strlen(buffer), "x", &skin, error, sizeof error), "missing delete");
    check_text(error, "The skin is missing the color \"delete\".", "missing color error");

    snprintf(buffer, sizeof buffer,
             "{\"name\":\"A\",\"colors\":{\"background\":\"#000000\",\"selection\":\"#FFFFFF\","
             "\"foreground\":\"#FFFFFF\",\"muted\":\"#888888\",\"delete\":\"nope\"}}");
    check(!c4p_skin_from_json(buffer, strlen(buffer), "x", &skin, error, sizeof error), "bad colour");
    check_text(error, "Color \"delete\" is not a valid #RRGGBB or #RRGGBBAA value.", "bad colour error");

    snprintf(buffer, sizeof buffer, "{\"name\":\"A\",%s,\"appearance\":\"midnight\"}", good_colors);
    check(!c4p_skin_from_json(buffer, strlen(buffer), "x", &skin, error, sizeof error), "bad appearance");
    check_text(error, "\"appearance\" must be \"dark\" or \"light\" (got \"midnight\").", "appearance error");

    snprintf(buffer, sizeof buffer, "{\"name\":\"A\",%s,\"cornerRadius\":31}", good_colors);
    check(!c4p_skin_from_json(buffer, strlen(buffer), "x", &skin, error, sizeof error), "bad radius");
    check_text(error, "\"cornerRadius\" must be a number between 0 and 30.", "radius error");

    snprintf(buffer, sizeof buffer, "{\"name\":\"A\",%s,\"font\":\"Menlo\"}", good_colors);
    check(!c4p_skin_from_json(buffer, strlen(buffer), "x", &skin, error, sizeof error), "bad font");
    check_text(error, "\"font\" must be an object with an optional \"name\" and \"size\".", "font error");

    snprintf(buffer, sizeof buffer, "{\"name\":\"A\",%s,\"font\":{\"size\":99}}", good_colors);
    check(c4p_skin_from_json(buffer, strlen(buffer), "x", &skin, error, sizeof error), "size out of range ignored");
    check(skin.font_size == 0.0, "font size 99 dropped");
}

static void test_ids(void) {
    char id[96];
    c4p_skin_safe_id("Sunset.json", id, sizeof id);
    check_text(id, "Sunset", "plain name");
    c4p_skin_safe_id("../bad name!.JSON", id, sizeof id);
    check_text(id, "bad name", "path and punctuation removed");
    c4p_skin_safe_id(".json", id, sizeof id);
    check_text(id, "skin", "empty name falls back to skin");
    c4p_skin_safe_id("   ", id, sizeof id);
    check_text(id, "skin", "blank name falls back to skin");
}

static void test_import_and_list(void) {
    char dir_template[256] = {0};
#ifdef _WIN32
    snprintf(dir_template, sizeof dir_template, "%s\\c4p-test", getenv("TEMP") ? getenv("TEMP") : ".");
    char mkdir_cmd[300];
    snprintf(mkdir_cmd, sizeof mkdir_cmd, "mkdir \"%s\" 2>NUL", dir_template);
    system(mkdir_cmd);
#else
    snprintf(dir_template, sizeof dir_template, "/tmp/c4p-test-XXXXXX");
    char *made = mkdtemp(dir_template);
    check(made != NULL, "temp directory created");
#endif
    char source[512];
#ifdef _WIN32
    snprintf(source, sizeof source, "%s\\source.json", dir_template);
#else
    snprintf(source, sizeof source, "%s/source.json", dir_template);
#endif

    const char *valid =
        "{\"name\":\"Mine\",\"colors\":{\"background\":\"#101010\",\"selection\":\"#FFFFFF33\","
        "\"foreground\":\"#FFFFFF\",\"muted\":\"#999999\",\"delete\":\"#EEEEEE\"}}";
    check(c4p_write_file_atomic(source, valid, strlen(valid)), "source skin written");

    char skins_dir[600];
    join(skins_dir, sizeof skins_dir, dir_template, "skins");

    char identifier[96];
    char error[256];
    check(c4p_skin_import(skins_dir, source, identifier, sizeof identifier, error, sizeof error),
          "skin imported");
    check_text(identifier, "source", "id comes from the file name");

    char imported_path[800];
    join(imported_path, sizeof imported_path, skins_dir, "source.json");
    char *imported_contents = c4p_read_file(imported_path, NULL);
    check(imported_contents != NULL, "imported file exists");
    free(imported_contents);

    c4p_skin_list list;
    c4p_skin_list_init(&list);
    c4p_skin_list_load(&list, skins_dir);
    check(list.count == 5, "four built-ins plus one imported");
    check_text(list.items[4].name, "Mine", "imported skin is listed");
    const c4p_skin *resolved = c4p_skin_list_resolve(&list, "source");
    check_text(resolved->name, "Mine", "resolve by id");
    resolved = c4p_skin_list_resolve(&list, "does-not-exist");
    check_text(resolved->id, "dark", "unknown id falls back to dark");

    /* A built-in name must not be shadowed by an import. */
    char builtin_source[600];
    join(builtin_source, sizeof builtin_source, dir_template, "dark.json");
    check(c4p_write_file_atomic(builtin_source, valid, strlen(valid)), "collision source written");
    check(c4p_skin_import(skins_dir, builtin_source, identifier, sizeof identifier, error, sizeof error),
          "collision import works");
    check_text(identifier, "dark-custom", "built-in id protected");

    /* Broken files are skipped, never fatal. */
    char broken_path[600];
    join(broken_path, sizeof broken_path, skins_dir, "broken.json");
    check(c4p_write_file_atomic(broken_path, "{\"name\":}", 10), "broken file written");
    c4p_skin_list reloaded;
    c4p_skin_list_init(&reloaded);
    c4p_skin_list_load(&reloaded, skins_dir);
    check(reloaded.count == 6, "broken file skipped (4 built-ins + 2 imported)");
    check_text(reloaded.items[4].name, "Mine", "imports sorted by name");
    c4p_skin_list_free(&reloaded);
    c4p_skin_list_free(&list);

    char missing[600];
    join(missing, sizeof missing, dir_template, "nope.json");
    check(!c4p_skin_import(skins_dir, missing, identifier, sizeof identifier, error, sizeof error),
          "missing file rejected");
    check_text(error, "The selected file could not be read.", "missing file error");

    /* Cleanup is best effort; tests never touch the user's own folders. */
#ifndef _WIN32
    char cleanup[600];
    snprintf(cleanup, sizeof cleanup,
             "rm -rf '%s'", dir_template);
    if (system(cleanup) != 0) fprintf(stderr, "note: could not remove %s\n", dir_template);
#endif
}

static void test_history(void) {
    c4p_history history;
    c4p_history_init(&history, 4);
    check(history.capacity == 4, "capacity 4");
    check(c4p_history_remember(&history, "one"), "remember one");
    check(c4p_history_remember(&history, "two"), "remember two");
    check(c4p_history_remember(&history, "three"), "remember three");
    check(history.count == 3, "three entries");
    check_text(history.items[0], "three", "newest first");

    check(c4p_history_remember(&history, "one"), "duplicate accepted");
    check(history.count == 3, "duplicate is not counted twice");
    check_text(history.items[0], "one", "duplicate moves to the top");
    check_text(history.items[1], "three", "order otherwise kept");

    check(!c4p_history_remember(&history, ""), "empty text ignored");
    check(!c4p_history_remember(&history, NULL), "NULL ignored");

    check(c4p_history_remember(&history, "four"), "remember four");
    check(c4p_history_remember(&history, "five"), "remember five");
    check(history.count == 4, "capacity respected");
    check_text(history.items[0], "five", "newest after trim");
    check_text(history.items[3], "three", "oldest kept in window");

    check(c4p_history_remove(&history, "four"), "remove existing");
    check(!c4p_history_remove(&history, "four"), "remove twice is a no-op");
    check(history.count == 3, "count after remove");
    check_text(history.items[0], "five", "remove keeps order");
    check_text(history.items[2], "three", "last entry intact");

    c4p_history_set_capacity(&history, 2);
    check(history.count == 2, "shrinking trims");
    c4p_history_set_capacity(&history, 99);
    check(history.capacity == C4P_CAPACITY_MAX, "capacity clamped to 30");
    c4p_history_free(&history);
    check(history.count == 0, "free empties");

    c4p_history big;
    c4p_history_init(&big, 4);
    char *huge = (char *)malloc(C4P_MAX_TEXT_BYTES + 2);
    memset(huge, 'x', C4P_MAX_TEXT_BYTES + 1);
    huge[C4P_MAX_TEXT_BYTES + 1] = '\0';
    check(!c4p_history_remember(&big, huge), "over 2 MB rejected");
    huge[C4P_MAX_TEXT_BYTES] = '\0';
    check(c4p_history_remember(&big, huge), "exactly 2 MB accepted");
    free(huge);
    c4p_history_free(&big);
}

static void test_preview(void) {
    char *text = c4p_preview_text("line one\nline two\tend");
    check_text(text, "line one \xE2\x86\xB5 line two \xE2\x86\xA5 end", "preview keeps one line");
    free(text);
    text = c4p_preview_text("a\r\nb");
    check_text(text, "a  \xE2\x86\xB5 b", "carriage returns become spaces");
    free(text);
    check(c4p_preview_text(NULL) == NULL, "NULL preview");
}

static void test_config(void) {
    c4p_config config;
    c4p_config_defaults(&config);
    check(config.capacity == 4 && config.opacity == 94, "defaults");
    check_text(config.skin, "dark", "default skin");

    char path[256];
#ifdef _WIN32
    const char *temp = getenv("TEMP") ? getenv("TEMP") : ".";
    snprintf(path, sizeof path, "%s\\c4p-settings-test.ini", temp);
#else
    snprintf(path, sizeof path, "/tmp/c4p-settings-test.ini");
#endif
    config.capacity = 7;
    config.opacity = 60;
    snprintf(config.skin, sizeof config.skin, "ocean");
    check(c4p_config_save(&config, path), "config saved");

    c4p_config loaded;
    check(c4p_config_load(&loaded, path), "config loaded");
    check(loaded.capacity == 7 && loaded.opacity == 60, "values round-trip");
    check_text(loaded.skin, "ocean", "skin round-trip");

    const char *bad = "capacity=500\nskin=\nopacity=1\n";
    check(c4p_write_file_atomic(path, bad, strlen(bad)), "bad config written");
    check(c4p_config_load(&loaded, path), "bad config still loads");
    check(loaded.capacity == 30 && loaded.opacity == 20, "bad values clamped");
    check_text(loaded.skin, "dark", "empty skin falls back to dark");

    check(!c4p_config_load(&loaded, "/nonexistent/c4p.ini"), "missing config reports 0");
    check(loaded.capacity == 4, "missing config keeps defaults");
    remove(path);
}

int main(void) {
    test_colors();
    test_builtins();
    test_parse_ok();
    test_parse_errors();
    test_ids();
    test_import_and_list();
    test_history();
    test_preview();
    test_config();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("copy4paste core OK: colours, skins, imports, history, config\n");
    return 0;
}
