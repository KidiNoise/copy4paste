/* copy4paste for Linux (GTK 3 + X11).
 *
 * Same product as the macOS build: Ctrl+V opens a small picker with your last
 * copies, digits/arrows/Enter choose one, Backspace deletes, Esc closes. Skins
 * are the same JSON files, read by the shared core in shared/.
 *
 * Nothing is written to disk except the preferences file and imported skins.
 * No network, no telemetry. The history lives in RAM.
 */
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#endif

#include "c4p.h"

#define PANEL_WIDTH C4P_PANEL_WIDTH
#define ROW_HEIGHT C4P_ROW_HEIGHT
#define HEADER_HEIGHT C4P_HEADER_HEIGHT
#define VISIBLE_ROWS C4P_VISIBLE_ROWS
#define DISMISS_MS 12000
#define PASTE_DELAY_MS 150

static c4p_config config;
static c4p_skin_list skins;
static c4p_history history;
static char *config_path;
static char *skins_dir;

static GtkWidget *panel;
static GtkWidget *panel_area;
static GtkWidget *settings_window;

static int panel_visible;
static int selected_index;
static int first_row;
static int displayed_count;

static guint dismiss_timer;
static guint paste_timer;

static char *self_set_text; /* text we put on the clipboard ourselves */
static char *forced_skin;   /* --skin, for screenshots and tests */

static int self_test;
static int verify_mode;
static int self_test_seconds = 15;

/* One server per session: --select and --settings talk to the running app,
 * because the clipboard history only exists in that process's memory. */
static int g_server_fd = -1;
static char g_socket_path[108];

/* 0 = not tried yet, 1 = grabbed, 2 = somebody else owns Ctrl+V */
static int g_hotkey_state;
static int g_ctrl_alt_p_state;
static int g_wayland;

#if defined(GDK_WINDOWING_X11)
static Display *x_display;
static Window x_root;
static KeyCode key_v;
static KeyCode key_p;
static int key_grab_ok;
static int keyboard_grabbed;
static int xtest_ok;
#endif

static void hide_panel(void);
static void open_settings(void);
static void show_panel_at_pointer(void);

/* --- helpers ---------------------------------------------------------- */

static const c4p_skin *current_skin(void) {
    const char *id = forced_skin ? forced_skin : config.skin;
    return c4p_skin_list_resolve(&skins, id);
}

static int visible_rows(void) {
    int visible = displayed_count < VISIBLE_ROWS ? displayed_count : VISIBLE_ROWS;
    return visible < 1 ? 1 : visible;
}

static int panel_height(void) { return HEADER_HEIGHT + ROW_HEIGHT * visible_rows(); }

static void queue_panel_redraw(void) {
    if (panel_area) gtk_widget_queue_draw(panel_area);
}

static void rounded_path(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_path(cr);
    cairo_move_to(cr, x + r, y);
    cairo_line_to(cr, x + w - r, y);
    cairo_curve_to(cr, x + w, y, x + w, y, x + w, y + r);
    cairo_line_to(cr, x + w, y + h - r);
    cairo_curve_to(cr, x + w, y + h, x + w, y + h, x + w - r, y + h);
    cairo_line_to(cr, x + r, y + h);
    cairo_curve_to(cr, x, y + h, x, y + h, x, y + h - r);
    cairo_line_to(cr, x, y + r);
    cairo_curve_to(cr, x, y, x, y, x + r, y);
    cairo_close_path(cr);
}

static PangoLayout *make_layout(cairo_t *cr, const char *text, const char *font,
                                const c4p_color *color, int width) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, text ? text : "", -1);
    PangoFontDescription *description = pango_font_description_from_string(font);
    pango_layout_set_font_description(layout, description);
    pango_font_description_free(description);
    if (width > 0) {
        pango_layout_set_width(layout, width * PANGO_SCALE);
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    }
    pango_layout_set_single_paragraph_mode(layout, TRUE);
    if (color) cairo_set_source_rgba(cr, color->r, color->g, color->b, color->a);
    return layout;
}

static void draw_layout(cairo_t *cr, PangoLayout *layout, double x, double y) {
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

static int skin_font_size(const c4p_skin *skin) {
    return (int)lround(skin->font_size > 0 ? skin->font_size : 12);
}

static char *row_font(const c4p_skin *skin) {
    char *font = (char *)malloc(192);
    if (!font) return NULL;
    if (skin->font_name[0]) {
        snprintf(font, 192, "%s %d", skin->font_name, skin_font_size(skin));
    } else {
        snprintf(font, 192, "Sans %d", skin_font_size(skin));
    }
    return font;
}

/* --- clipboard -------------------------------------------------------- */

static void clear_self_set(void) {
    free(self_set_text);
    self_set_text = NULL;
}

static void set_clipboard_text(const char *text) {
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    clear_self_set();
    self_set_text = text ? g_strdup(text) : NULL;
    gtk_clipboard_set_text(clipboard, text ? text : "", -1);
    gtk_clipboard_store(clipboard);
}

static void on_clipboard_text(GtkClipboard *clipboard, const gchar *text, gpointer data) {
    (void)clipboard;
    (void)data;
    if (!text || !*text) return;
    if (self_set_text && strcmp(text, self_set_text) == 0) return;
    if (strncmp(text, "file://", 7) == 0) return; /* copied files are not text */
    if (strlen(text) > C4P_MAX_TEXT_BYTES) return;
    c4p_history_remember(&history, text);
}

static void on_owner_change(GtkClipboard *clipboard, GdkEvent *event, gpointer data) {
    (void)event;
    (void)data;
    GdkAtom *targets = NULL;
    gint count = 0;
    if (gtk_clipboard_wait_for_targets(clipboard, &targets, &count)) {
        int skip = 0;
        for (gint i = 0; i < count; i++) {
            char *name = gdk_atom_name(targets[i]);
            if (name && (strcmp(name, "text/uri-list") == 0 || strcmp(name, "image/png") == 0 ||
                         strcmp(name, "image/bmp") == 0 || strcmp(name, "image/tiff") == 0)) {
                skip = 1;
            }
            g_free(name);
        }
        g_free(targets);
        if (skip) return;
    }
    gtk_clipboard_request_text(clipboard, on_clipboard_text, NULL);
}

/* --- X11: key grab, keys, synthetic paste ----------------------------- */

#if defined(GDK_WINDOWING_X11)

static int g_grab_errors;

static int grab_error_handler(Display *display, XErrorEvent *error) {
    (void)display;
    if (error->error_code == BadAccess) g_grab_errors++;
    return 0;
}

static void ungrab_keys(void) {
    if (!x_display || !key_grab_ok) return;
    XUngrabKey(x_display, key_v, AnyModifier, x_root);
    XUngrabKey(x_display, key_p, AnyModifier, x_root);
    XSync(x_display, False);
}

static void grab_keys(void) {
    if (!x_display || !key_grab_ok) return;
    unsigned int locks[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask,
                             Mod5Mask, LockMask | Mod5Mask, Mod2Mask | Mod5Mask,
                             LockMask | Mod2Mask | Mod5Mask };
    XErrorHandler previous = XSetErrorHandler(grab_error_handler);
    size_t i;
    for (i = 0; i < sizeof locks / sizeof locks[0]; i++) {
        /* BadAccess here means another client already owns this combination:
         * the desktop, a keyboard manager, or another copy4paste. */
        g_grab_errors = 0;
        XGrabKey(x_display, key_v, ControlMask | locks[i], x_root, False, GrabModeAsync, GrabModeAsync);
        XSync(x_display, False);
        if (g_grab_errors) g_hotkey_state = 2;
        else if (g_hotkey_state == 0) g_hotkey_state = 1;

        g_grab_errors = 0;
        XGrabKey(x_display, key_p, ControlMask | Mod1Mask | locks[i], x_root, False, GrabModeAsync,
                 GrabModeAsync);
        XSync(x_display, False);
        if (g_grab_errors) g_ctrl_alt_p_state = 2;
        else if (g_ctrl_alt_p_state == 0) g_ctrl_alt_p_state = 1;
    }
    XSetErrorHandler(previous);
    XSync(x_display, False);
}

static void grab_keyboard(void) {
    if (!x_display) return;
    keyboard_grabbed = XGrabKeyboard(x_display, x_root, False, GrabModeAsync, GrabModeAsync,
                                     CurrentTime) == GrabSuccess;
    XSync(x_display, False);
}

static void release_keyboard(void) {
    if (!x_display || !keyboard_grabbed) return;
    XUngrabKeyboard(x_display, CurrentTime);
    keyboard_grabbed = 0;
    XSync(x_display, False);
}

/* Sends Ctrl+V to whichever window has the focus. Our own grab is dropped
 * first, otherwise the synthetic key would come back to us. */
static void inject_paste(void) {
    if (!x_display || !xtest_ok) return;
    ungrab_keys();
    release_keyboard();
    KeyCode control = XKeysymToKeycode(x_display, XK_Control_L);
    KeyCode v = XKeysymToKeycode(x_display, XK_v);
    XTestFakeKeyEvent(x_display, control, True, CurrentTime);
    XTestFakeKeyEvent(x_display, v, True, CurrentTime);
    XTestFakeKeyEvent(x_display, v, False, CurrentTime);
    XTestFakeKeyEvent(x_display, control, False, CurrentTime);
    XSync(x_display, False);
    grab_keys();
}

#endif

static gboolean paste_now(void *unused) {
    (void)unused;
    paste_timer = 0;
#if defined(GDK_WINDOWING_X11)
    inject_paste();
#endif
    return G_SOURCE_REMOVE;
}

static void schedule_paste(void) { paste_timer = g_timeout_add(PASTE_DELAY_MS, paste_now, NULL); }

/* --- panel drawing ---------------------------------------------------- */

static void draw_panel(GtkWidget *widget, cairo_t *cr, gpointer data) {
    (void)data;
    const c4p_skin *skin = current_skin();
    int width = PANEL_WIDTH;
    int height = panel_height();
    GdkScreen *screen = gtk_widget_get_screen(widget);
    int composited = screen && gdk_screen_is_composited(screen);

    double scale = gtk_widget_get_scale_factor(widget);
    cairo_scale(cr, scale, scale);

    if (composited) {
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    }

    double alpha = (double)config.opacity / 100.0 * skin->background.a;
    if (!composited) alpha = 1.0;
    if (composited && skin->corner_radius > 0.5) {
        rounded_path(cr, 0, 0, width, height, skin->corner_radius);
    } else {
        cairo_rectangle(cr, 0, 0, width, height);
    }
    cairo_set_source_rgba(cr, skin->background.r, skin->background.g, skin->background.b, alpha);
    cairo_fill(cr);

    draw_layout(cr, make_layout(cr, "copy4paste", "Sans 10", &skin->muted, 0), 10, 5);

    char hint[64];
    if (displayed_count > VISIBLE_ROWS) {
        snprintf(hint, sizeof hint, "%d\xE2\x80\x93%d/%d", first_row + 1, first_row + visible_rows(),
                 displayed_count);
    } else {
        snprintf(hint, sizeof hint, "1 = latest");
    }
    PangoLayout *hint_layout = make_layout(cr, hint, "Sans 9", &skin->muted, 0);
    int hint_width = 0;
    pango_layout_get_pixel_size(hint_layout, &hint_width, NULL);
    draw_layout(cr, hint_layout, width - 46 - hint_width, 5);

    draw_layout(cr, make_layout(cr, "\xE2\x9A\x99", "Sans 13", &skin->muted, 0), width - 34, 2);

    char *font = row_font(skin);
    int visible = visible_rows();
    int i;
    for (i = 0; i < visible; i++) {
        int absolute = first_row + i;
        if (absolute >= displayed_count) break;
        double y = 25 + ROW_HEIGHT * i;

        if (absolute == selected_index) {
            cairo_set_source_rgba(cr, skin->selection.r, skin->selection.g, skin->selection.b,
                                  skin->selection.a);
            rounded_path(cr, 6, y + 1, width - 12, ROW_HEIGHT - 2, 5);
            cairo_fill(cr);
        }

        char number[16];
        snprintf(number, sizeof number, "%d", absolute + 1);
        char number_font[48];
        snprintf(number_font, sizeof number_font, "Monospace %s11", absolute == 0 ? "Bold " : "");
        draw_layout(cr, make_layout(cr, number, number_font, &skin->muted, 0), 9, y + 7);

        char *preview = c4p_preview_text(history.items[absolute]);
        draw_layout(cr, make_layout(cr, preview ? preview : "", font, &skin->foreground, PANEL_WIDTH - 67),
                    29, y + 6);
        free(preview);

        double cx = width - 18;
        double cy = y + 15.5;
        cairo_set_source_rgba(cr, skin->delete_color.r, skin->delete_color.g, skin->delete_color.b,
                              skin->delete_color.a);
        cairo_set_line_width(cr, 1.7);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, cx - 4.3, cy - 4.3);
        cairo_line_to(cr, cx + 4.3, cy + 4.3);
        cairo_move_to(cr, cx - 4.3, cy + 4.3);
        cairo_line_to(cr, cx + 4.3, cy - 4.3);
        cairo_stroke(cr);
    }
    free(font);
}

static int row_at(double x, double y) {
    if (y < 25) return -1;
    int local = (int)((y - 25) / ROW_HEIGHT);
    if (local < 0 || local >= visible_rows()) return -1;
    if (x < 5 || x > PANEL_WIDTH - 5) return -1;
    int absolute = first_row + local;
    if (absolute >= displayed_count) return -1;
    return absolute;
}

static int cross_at(double x, double y) {
    int row = row_at(x, y);
    if (row < 0) return -1;
    int local = row - first_row;
    double top = 25 + ROW_HEIGHT * local + 4;
    if (x >= PANEL_WIDTH - 32 && y >= top && y <= top + ROW_HEIGHT - 8) return row;
    return -1;
}

static gboolean gear_at(double x, double y) {
    return x >= PANEL_WIDTH - 40 && x <= PANEL_WIDTH - 4 && y >= 0 && y <= 24;
}

/* --- panel behaviour -------------------------------------------------- */

static void clamp_first_row(void) {
    int maximum = displayed_count - visible_rows();
    if (maximum < 0) maximum = 0;
    if (first_row > maximum) first_row = maximum;
    if (first_row < 0) first_row = 0;
}

static void layout_panel(void) {
    int height = panel_height();
    gtk_widget_set_size_request(panel_area, PANEL_WIDTH, height);
    gtk_window_resize(GTK_WINDOW(panel), PANEL_WIDTH, height);
}

static void update_selection(void) {
    if (displayed_count <= 0) return;
    if (selected_index < 0) selected_index = 0;
    if (selected_index >= displayed_count) selected_index = displayed_count - 1;
    int visible = visible_rows();
    if (selected_index < first_row) first_row = selected_index;
    if (selected_index >= first_row + visible) first_row = selected_index - visible + 1;
    clamp_first_row();
    queue_panel_redraw();
}

static gboolean hide_panel_forward(void *unused) {
    (void)unused;
    dismiss_timer = 0;
    hide_panel();
    return G_SOURCE_REMOVE;
}

static void schedule_dismiss(void) {
    if (dismiss_timer) g_source_remove(dismiss_timer);
    dismiss_timer = g_timeout_add(DISMISS_MS, hide_panel_forward, NULL);
}

static void ensure_panel(void) {
    if (panel) return;
    panel = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_keep_above(GTK_WINDOW(panel), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(panel), FALSE);
    gtk_window_set_focus_on_map(GTK_WINDOW(panel), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(panel), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(panel), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(panel), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
    gtk_widget_set_app_paintable(panel, TRUE);

    GdkScreen *screen = gtk_widget_get_screen(panel);
    if (screen && gdk_screen_is_composited(screen)) {
        GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
        if (visual) gtk_widget_set_visual(panel, visual);
    }

    panel_area = gtk_drawing_area_new();
    gtk_widget_add_events(panel_area, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK |
                                          GDK_LEAVE_NOTIFY_MASK | GDK_KEY_PRESS_MASK);
    gtk_container_add(GTK_CONTAINER(panel), panel_area);
    g_signal_connect(panel_area, "draw", G_CALLBACK(draw_panel), NULL);
    g_signal_connect(panel, "destroy", G_CALLBACK(gtk_widget_destroyed), &panel);
}

static void panel_geometry(int *out_x, int *out_y) {
    GdkDisplay *display = gdk_display_get_default();
    GdkSeat *seat = gdk_display_get_default_seat(display);
    GdkDevice *pointer = gdk_seat_get_pointer(seat);
    int px = 0, py = 0;
    gdk_device_get_position(pointer, NULL, &px, &py);
    GdkMonitor *monitor = gdk_display_get_monitor_at_point(display, px, py);
    if (!monitor) monitor = gdk_display_get_primary_monitor(display);
    GdkRectangle work = { 0, 0, 1024, 768 };
    if (monitor) gdk_monitor_get_workarea(monitor, &work);
    int height = panel_height();
    int x = px + 9;
    int y = py - height + 8;
    if (x > work.x + work.width - PANEL_WIDTH - 6) x = work.x + work.width - PANEL_WIDTH - 6;
    if (x < work.x + 6) x = work.x + 6;
    if (y > work.y + work.height - height - 6) y = work.y + work.height - height - 6;
    if (y < work.y + 6) y = work.y + 6;
    *out_x = x;
    *out_y = y;
}

static void show_panel_at_pointer(void) {
    if (history.count < 2) return;
    ensure_panel();
    selected_index = 0;
    first_row = 0;
    displayed_count = history.count;
    panel_visible = 1;
    layout_panel();
    int x = 0, y = 0;
    panel_geometry(&x, &y);
    gtk_window_move(GTK_WINDOW(panel), x, y);
    gtk_widget_show_all(panel);
    GdkWindow *window = gtk_widget_get_window(panel);
    if (window) gdk_window_raise(window);
#if defined(GDK_WINDOWING_X11)
    grab_keyboard();
#endif
    schedule_dismiss();
    queue_panel_redraw();
}

static void hide_panel(void) {
    if (dismiss_timer) {
        g_source_remove(dismiss_timer);
        dismiss_timer = 0;
    }
#if defined(GDK_WINDOWING_X11)
    release_keyboard();
#endif
    if (panel) gtk_widget_hide(panel);
    panel_visible = 0;
    selected_index = 0;
    first_row = 0;
}

static void choose_index(int index) {
    if (!panel_visible || index < 0 || index >= history.count) {
        hide_panel();
        return;
    }
    char *text = g_strdup(history.items[index]);
    set_clipboard_text(text);
    g_free(text);
    hide_panel();
    schedule_paste();
}

static void delete_index(int index) {
    if (index < 0 || index >= history.count) return;
    char *text = g_strdup(history.items[index]);
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gchar *current = gtk_clipboard_wait_for_text(clipboard);
    gboolean was_clipboard = current && strcmp(current, text) == 0;
    g_free(current);

    c4p_history_remove(&history, text);
    if (was_clipboard) {
        if (history.count > 0) set_clipboard_text(history.items[0]);
        else {
            clear_self_set();
            gtk_clipboard_clear(clipboard);
        }
    }
    g_free(text);
    displayed_count = history.count;
    if (history.count < 2) {
        hide_panel();
        return;
    }
    if (selected_index >= displayed_count) selected_index = displayed_count - 1;
    update_selection();
    layout_panel();
    queue_panel_redraw();
    schedule_dismiss();
}

static gboolean handle_key(unsigned int keyval) {
    if (!panel_visible) return FALSE;
    switch (keyval) {
        case GDK_KEY_Escape:
            hide_panel();
            return TRUE;
        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
            selected_index = (selected_index + 1) % displayed_count;
            update_selection();
            schedule_dismiss();
            return TRUE;
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
            selected_index = (selected_index - 1 + displayed_count) % displayed_count;
            update_selection();
            schedule_dismiss();
            return TRUE;
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_ISO_Enter:
            choose_index(selected_index);
            return TRUE;
        case GDK_KEY_BackSpace:
        case GDK_KEY_Delete:
            delete_index(selected_index);
            return TRUE;
        default:
            if (keyval >= GDK_KEY_1 && keyval <= GDK_KEY_9) {
                int index = (int)(keyval - GDK_KEY_1);
                if (index < displayed_count) {
                    choose_index(index);
                    return TRUE;
                }
            }
            if (keyval >= GDK_KEY_KP_1 && keyval <= GDK_KEY_KP_9) {
                int index = (int)(keyval - GDK_KEY_KP_1);
                if (index < displayed_count) {
                    choose_index(index);
                    return TRUE;
                }
            }
            break;
    }
    return FALSE;
}

static gboolean on_panel_key(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;
    (void)data;
    return handle_key(event->keyval);
}

static gboolean on_panel_button(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget;
    (void)data;
    if (event->button != 1) return FALSE;
    if (gear_at(event->x, event->y)) {
        open_settings();
        return TRUE;
    }
    int cross = cross_at(event->x, event->y);
    if (cross >= 0) {
        delete_index(cross);
        return TRUE;
    }
    int row = row_at(event->x, event->y);
    if (row >= 0) {
        choose_index(row);
        return TRUE;
    }
    hide_panel();
    return TRUE;
}

static gboolean on_panel_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    (void)widget;
    (void)data;
    int row = row_at(event->x, event->y);
    if (row >= 0 && row != selected_index) {
        selected_index = row;
        queue_panel_redraw();
        schedule_dismiss();
    }
    return FALSE;
}

static gboolean on_panel_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer data) {
    (void)widget;
    (void)data;
    if (displayed_count <= VISIBLE_ROWS) return FALSE;
    int delta = 0;
    if (event->direction == GDK_SCROLL_DOWN) delta = 1;
    else if (event->direction == GDK_SCROLL_UP) delta = -1;
    else if (event->direction == GDK_SCROLL_SMOOTH) delta = event->delta_y > 0 ? 1 : -1;
    if (!delta) return FALSE;
    int maximum = displayed_count - VISIBLE_ROWS;
    int next = first_row + delta;
    if (next < 0) next = 0;
    if (next > maximum) next = maximum;
    if (next == first_row) return TRUE;
    first_row = next;
    selected_index = next;
    queue_panel_redraw();
    schedule_dismiss();
    return TRUE;
}

/* Wire the events once the panel exists: called from show and from tests. */
static void connect_panel_events(void) {
    g_signal_connect(panel_area, "button-press-event", G_CALLBACK(on_panel_button), NULL);
    g_signal_connect(panel_area, "motion-notify-event", G_CALLBACK(on_panel_motion), NULL);
    g_signal_connect(panel_area, "scroll-event", G_CALLBACK(on_panel_scroll), NULL);
    g_signal_connect(panel, "key-press-event", G_CALLBACK(on_panel_key), NULL);
}

/* --- settings --------------------------------------------------------- */

static void apply_settings_css(GtkWidget *window, const c4p_skin *skin) {
    char css[900];
    snprintf(css, sizeof css,
             "window { background-color: rgb(%d,%d,%d); }"
             "label { color: rgb(%d,%d,%d); font-size: 12px; }"
             "#c4p-title { font-weight: 600; font-size: 13px; }"
             "#c4p-help { color: rgb(%d,%d,%d); font-size: 10px; }"
             "button { color: rgb(%d,%d,%d); }",
             (int)lround(skin->background.r * 255), (int)lround(skin->background.g * 255),
             (int)lround(skin->background.b * 255), (int)lround(skin->foreground.r * 255),
             (int)lround(skin->foreground.g * 255), (int)lround(skin->foreground.b * 255),
             (int)lround(skin->muted.r * 255), (int)lround(skin->muted.g * 255),
             (int)lround(skin->muted.b * 255), (int)lround(skin->foreground.r * 255),
             (int)lround(skin->foreground.g * 255), (int)lround(skin->foreground.b * 255));
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(window), GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void rebuild_settings(void) {
    if (settings_window) {
        gtk_widget_destroy(settings_window);
        settings_window = NULL;
    }
    open_settings();
}

static void on_skin_changed(GtkComboBox *combo, gpointer data) {
    (void)data;
    int index = gtk_combo_box_get_active(combo);
    if (index < 0 || index >= skins.count) return;
    snprintf(config.skin, sizeof config.skin, "%s", skins.items[index].id);
    free(forced_skin);
    forced_skin = NULL;
    c4p_config_save(&config, config_path);
    queue_panel_redraw();
    rebuild_settings();
}

static void on_capacity_changed(GtkSpinButton *spin, gpointer data) {
    (void)data;
    int value = gtk_spin_button_get_value_as_int(spin);
    config.capacity = value;
    c4p_history_set_capacity(&history, value);
    c4p_config_save(&config, config_path);
    if (panel_visible) {
        displayed_count = history.count;
        if (history.count < 2) hide_panel();
        else {
            update_selection();
            layout_panel();
            queue_panel_redraw();
        }
    }
}

static void on_opacity_changed(GtkSpinButton *spin, gpointer data) {
    (void)data;
    config.opacity = gtk_spin_button_get_value_as_int(spin);
    c4p_config_save(&config, config_path);
    queue_panel_redraw();
}

static void on_import_clicked(GtkButton *button, gpointer data) {
    (void)button;
    (void)data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Choose a copy4paste skin", GTK_WINDOW(settings_window), GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel",
        GTK_RESPONSE_CANCEL, "Import", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "copy4paste skins (*.json)");
    gtk_file_filter_add_pattern(filter, "*.json");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    char *filename = NULL;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    }
    gtk_widget_destroy(dialog);
    if (!filename) return;

    char identifier[96];
    char error[256];
    if (!c4p_skin_import(skins_dir, filename, identifier, sizeof identifier, error, sizeof error)) {
        GtkWidget *message = gtk_message_dialog_new(GTK_WINDOW(settings_window), GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                                    "Could not import skin");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message), "%s", error);
        gtk_dialog_run(GTK_DIALOG(message));
        gtk_widget_destroy(message);
        g_free(filename);
        return;
    }
    g_free(filename);
    c4p_skin_list_load(&skins, skins_dir);
    snprintf(config.skin, sizeof config.skin, "%s", identifier);
    c4p_config_save(&config, config_path);
    rebuild_settings();
}

static void on_close_settings(GtkButton *button, gpointer data) {
    (void)button;
    (void)data;
    if (settings_window) {
        gtk_widget_destroy(settings_window);
        settings_window = NULL;
    }
    show_panel_at_pointer();
}

#if defined(GDK_WINDOWING_X11)
static void ungrab_keys(void);
#endif

static void on_quit(GtkButton *button, gpointer data) {
    (void)button;
    (void)data;
    c4p_config_save(&config, config_path);
#if defined(GDK_WINDOWING_X11)
    ungrab_keys();
#endif
    gtk_main_quit();
}

static void open_settings(void) {
    if (panel_visible) hide_panel();
    if (settings_window) {
        gtk_window_present(GTK_WINDOW(settings_window));
        return;
    }
    const c4p_skin *skin = current_skin();

    settings_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(settings_window), "copy4paste \xC2\xB7 Settings \xC2\xB7 " C4P_VERSION);
    gtk_window_set_resizable(GTK_WINDOW(settings_window), FALSE);
    gtk_window_set_position(GTK_WINDOW(settings_window), GTK_WIN_POS_MOUSE);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 16);
    gtk_container_add(GTK_CONTAINER(settings_window), grid);

    GtkWidget *title = gtk_label_new("copy4paste \xC2\xB7 Settings \xC2\xB7 " C4P_VERSION);
    gtk_widget_set_name(title, "c4p-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), title, 0, 0, 2, 1);

    GtkWidget *copies_label = gtk_label_new("Copies");
    gtk_widget_set_halign(copies_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), copies_label, 0, 1, 1, 1);
    GtkWidget *copies = gtk_spin_button_new_with_range(C4P_CAPACITY_MIN, C4P_CAPACITY_MAX, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(copies), (double)config.capacity);
    gtk_widget_set_hexpand(copies, TRUE);
    gtk_widget_set_halign(copies, GTK_ALIGN_END);
    g_signal_connect(copies, "value-changed", G_CALLBACK(on_capacity_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid), copies, 1, 1, 1, 1);

    GtkWidget *skin_label = gtk_label_new("Skin");
    gtk_widget_set_halign(skin_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), skin_label, 0, 2, 1, 1);
    GtkWidget *combo = gtk_combo_box_text_new();
    int i;
    for (i = 0; i < skins.count; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), skins.items[i].name);
        if (strcmp(skins.items[i].id, skin->id) == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), i);
    }
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) < 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    g_signal_connect(combo, "changed", G_CALLBACK(on_skin_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid), combo, 1, 2, 1, 1);

    GtkWidget *import_button = gtk_button_new_with_label("Import JSON skin\xE2\x80\xA6");
    g_signal_connect(import_button, "clicked", G_CALLBACK(on_import_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), import_button, 1, 3, 1, 1);

    GtkWidget *opacity_label = gtk_label_new("Opacity");
    gtk_widget_set_halign(opacity_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), opacity_label, 0, 4, 1, 1);
    GtkWidget *opacity = gtk_spin_button_new_with_range(C4P_OPACITY_MIN, C4P_OPACITY_MAX, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(opacity), (double)config.opacity);
    gtk_widget_set_halign(opacity, GTK_ALIGN_END);
    g_signal_connect(opacity, "value-changed", G_CALLBACK(on_opacity_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid), opacity, 1, 4, 1, 1);

    GtkWidget *help = gtk_label_new("1\xE2\x80\x93" "30 copies \xC2\xB7 RAM only \xC2\xB7 skins live in ~/.config/copy4paste/skins");
    gtk_widget_set_name(help, "c4p-help");
    gtk_widget_set_halign(help, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), help, 0, 5, 2, 1);

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    gtk_widget_set_margin_top(buttons, 4);
    GtkWidget *quit = gtk_button_new_with_label("Quit");
    GtkWidget *close = gtk_button_new_with_label("Close");
    g_signal_connect(quit, "clicked", G_CALLBACK(on_quit), NULL);
    g_signal_connect(close, "clicked", G_CALLBACK(on_close_settings), NULL);
    gtk_box_pack_start(GTK_BOX(buttons), quit, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons), close, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), buttons, 0, 6, 2, 1);

    g_signal_connect(settings_window, "destroy", G_CALLBACK(gtk_widget_destroyed), &settings_window);
    apply_settings_css(settings_window, skin);
    gtk_widget_show_all(settings_window);
}

/* --- global shortcut -------------------------------------------------- */

#if defined(GDK_WINDOWING_X11)
static GdkFilterReturn x11_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data) {
    (void)event;
    (void)data;
    XEvent *x = (XEvent *)xevent;
    if (x->type != KeyPress) return GDK_FILTER_CONTINUE;
    XKeyEvent *key = &x->xkey;

    if (!panel_visible && key->keycode == key_v && (key->state & ControlMask) &&
        !(key->state & Mod1Mask)) {
        GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        gchar *text = gtk_clipboard_wait_for_text(clipboard);
        int has_text = text && *text;
        g_free(text);
        if (has_text && history.count > 1) {
            show_panel_at_pointer();
        } else {
            /* No history to choose from: behave like a normal Ctrl+V. */
            schedule_paste();
        }
        return GDK_FILTER_REMOVE;
    }
    if (!panel_visible && key->keycode == key_p && (key->state & ControlMask) && (key->state & Mod1Mask)) {
        open_settings();
        return GDK_FILTER_REMOVE;
    }
    if (panel_visible) {
        KeySym keysym = XkbKeycodeToKeysym(x_display, key->keycode, 0, 0);
        if (handle_key((unsigned int)keysym)) return GDK_FILTER_REMOVE;
    }
    return GDK_FILTER_CONTINUE;
}
#endif

/* --- one instance, and talking to it ---------------------------------- */

static void socket_path_build(void) {
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (runtime && *runtime) {
        snprintf(g_socket_path, sizeof g_socket_path, "%.80s/copy4paste.sock", runtime);
    } else {
        snprintf(g_socket_path, sizeof g_socket_path, "/tmp/c4p-%d.sock", (int)getuid());
    }
}

/*
 * Returns 1 and writes the reply when a copy4paste instance answered,
 * 0 when nothing is listening. The protocol is one line in, one line out,
 * so it stays readable with printf and easy to fake in a test.
 */
static int ipc_request(const char *command, char *reply, size_t reply_length) {
    if (reply && reply_length) reply[0] = '\0';
    socket_path_build();
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    struct sockaddr_un address;
    memset(&address, 0, sizeof address);
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, sizeof address.sun_path, "%s", g_socket_path);
    int ok = 0;
    if (connect(fd, (struct sockaddr *)&address, sizeof address) == 0) {
        char line[256];
        int length = snprintf(line, sizeof line, "%s\n", command);
        if (write(fd, line, (size_t)length) == length) {
            char buffer[1024];
            ssize_t got = read(fd, buffer, sizeof buffer - 1);
            if (got > 0) {
                buffer[got] = '\0';
                char *newline = strchr(buffer, '\n');
                if (newline) *newline = '\0';
                if (reply && reply_length) snprintf(reply, reply_length, "%s", buffer);
                ok = 1;
            }
        }
    }
    close(fd);
    return ok;
}

static void ipc_reply(int fd, const char *text) {
    char line[1200];
    int length = snprintf(line, sizeof line, "%s\n", text ? text : "ok");
    if (length > 0) {
        ssize_t written = write(fd, line, (size_t)length);
        (void)written;
    }
}

static gboolean deliver_select(void *unused) {
    (void)unused;
    show_panel_at_pointer();
    return G_SOURCE_REMOVE;
}

static gboolean deliver_settings(void *unused) {
    (void)unused;
    open_settings();
    return G_SOURCE_REMOVE;
}

static gboolean deliver_quit(void *unused) {
    (void)unused;
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

static gboolean on_client(GIOChannel *source, GIOCondition condition, gpointer data) {
    (void)data;
    if (!(condition & G_IO_IN)) return TRUE;
    int fd = g_io_channel_unix_get_fd(source);
    int client = accept(fd, NULL, NULL);
    if (client < 0) return TRUE;

    char buffer[256];
    ssize_t got = read(client, buffer, sizeof buffer - 1);
    if (got <= 0) {
        close(client);
        return TRUE;
    }
    buffer[got] = '\0';
    char *newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';

    if (strcmp(buffer, "select") == 0) {
        ipc_reply(client, "ok");
        close(client);
        g_idle_add(deliver_select, NULL);
    } else if (strcmp(buffer, "settings") == 0) {
        ipc_reply(client, "ok");
        close(client);
        g_idle_add(deliver_settings, NULL);
    } else if (strcmp(buffer, "quit") == 0) {
        ipc_reply(client, "ok");
        close(client);
        g_idle_add(deliver_quit, NULL);
    } else if (strcmp(buffer, "status") == 0) {
        const c4p_skin *skin = current_skin();
        char reply[600];
        snprintf(reply, sizeof reply,
                 "running=1 entries=%d skips=%d hotkey=%s settings_key=%s backend=%s skin=%s "
                 "panel_visible=%d capacity=%d opacity=%d",
                 history.count, skins.count,
                 g_hotkey_state == 1 ? "grabbed" : (g_hotkey_state == 2 ? "unavailable" : "unknown"),
                 g_ctrl_alt_p_state == 1 ? "grabbed"
                                         : (g_ctrl_alt_p_state == 2 ? "unavailable" : "unknown"),
                 g_wayland ? "wayland" : "x11", skin->id, panel_visible, config.capacity,
                 config.opacity);
        ipc_reply(client, reply);
        close(client);
    } else {
        ipc_reply(client, "error unknown-command");
        close(client);
    }
    return TRUE;
}

/* Returns 1 when this process is now the one the others talk to. */
static int ipc_serve_start(void) {
    socket_path_build();
    /* A socket left behind by a crash must not lock the user out. */
    if (access(g_socket_path, F_OK) == 0 && !ipc_request("status", NULL, 0)) unlink(g_socket_path);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    struct sockaddr_un address;
    memset(&address, 0, sizeof address);
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, sizeof address.sun_path, "%s", g_socket_path);
    if (bind(fd, (struct sockaddr *)&address, sizeof address) != 0) {
        close(fd);
        return 0;
    }
    chmod(g_socket_path, 0600);
    if (listen(fd, 8) != 0) {
        close(fd);
        unlink(g_socket_path);
        return 0;
    }
    g_server_fd = fd;
    GIOChannel *channel = g_io_channel_unix_new(fd);
    g_io_add_watch(channel, G_IO_IN, on_client, NULL);
    g_io_channel_unref(channel);
    return 1;
}

static void ipc_serve_stop(void) {
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
        unlink(g_socket_path);
    }
}

static void setup_hotkeys(void) {
#if defined(GDK_WINDOWING_X11)
    if (!GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        g_wayland = 1;
        g_hotkey_state = 2;
        g_ctrl_alt_p_state = 2;
        return;
    }
    x_display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    if (!x_display) return;
    x_root = DefaultRootWindow(x_display);
    key_v = XKeysymToKeycode(x_display, XK_v);
    key_p = XKeysymToKeycode(x_display, XK_p);
    int event_base = 0, error_base = 0;
    int major = 0, minor = 0;
    xtest_ok = XTestQueryExtension(x_display, &event_base, &error_base, &major, &minor);
    key_grab_ok = key_v != 0 && key_p != 0;
    grab_keys();
    gdk_window_add_filter(NULL, x11_filter, NULL);
    if (g_hotkey_state == 2) {
        fprintf(stderr,
                "copy4paste: another application already owns Ctrl+V, so the picker cannot open\n"
                "            from that key. Bind `copy4paste --select` to a shortcut you choose,\n"
                "            or quit whichever program is holding Ctrl+V.\n");
    }
    if (g_ctrl_alt_p_state == 2) {
        fprintf(stderr, "copy4paste: another application already owns Ctrl+Alt+P (settings).\n");
    }
#else
    g_hotkey_state = 2;
    g_ctrl_alt_p_state = 2;
#endif
}

/* --- tests ------------------------------------------------------------ */

static void write_log(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    if (!file) return;
    fputs(text, file);
    fclose(file);
}

static void seed_history(void) {
    c4p_history_remember(&history, "First sample copy");
    c4p_history_remember(&history, "Second sample copy");
    c4p_history_remember(&history, "Third sample copy with a much longer line that will be cut off");
    c4p_history_remember(&history, "Fourth sample copy");
}

static gboolean finish_self_test(void *unused) {
    (void)unused;
    char log[1024];
    const c4p_skin *skin = current_skin();
    snprintf(log, sizeof log,
             "VERSION=%s\nSKINS=%d\nSKIN=%s\nSKIN_NAME=%s\nLIGHT=%d\nRADIUS=%.1f\nENTRIES=%d\n"
             "PANEL_VISIBLE=%d\nPANEL_WIDTH=%d\nPANEL_HEIGHT=%d\nOPACITY=%d\nCAPACITY=%d\nFIRST=%s\n",
             C4P_VERSION, skins.count, skin->id, skin->name, skin->light, skin->corner_radius, history.count,
             panel_visible, PANEL_WIDTH, panel_height(), config.opacity, config.capacity,
             history.count > 0 ? history.items[0] : "");
    write_log("/tmp/copy4paste-linux-verify.log", log);
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

static gboolean finish_verify(void *unused) {
    (void)unused;
    char log[1200];
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gchar *text = gtk_clipboard_wait_for_text(clipboard);
    int correct = text && strcmp(text, history.items[1]) == 0 && !panel_visible;
    snprintf(log, sizeof log,
             "VERSION=%s\nENTRIES=%d\nPANEL_VISIBLE_AFTER_DIGIT=%d\nCLIPBOARD=%s\nRESULT=%s\n", C4P_VERSION,
             history.count, panel_visible, text ? text : "", correct ? "PASS" : "FAIL");
    write_log("/tmp/copy4paste-linux-verify.log", log);
    g_free(text);
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

#if defined(GDK_WINDOWING_X11)
/* Presses "2" through the X server, the same route a real keyboard takes. */
static gboolean verify_press_digit(void *unused) {
    (void)unused;
    if (!x_display || !xtest_ok) return G_SOURCE_REMOVE;
    KeyCode two = XKeysymToKeycode(x_display, XK_2);
    XTestFakeKeyEvent(x_display, two, True, CurrentTime);
    XTestFakeKeyEvent(x_display, two, False, CurrentTime);
    XSync(x_display, False);
    return G_SOURCE_REMOVE;
}
#endif

/* --- main ------------------------------------------------------------- */

int main(int argc, char **argv) {
    /* Answer --version without needing a display: CI and scripts use it. */
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("copy4paste %s \xC2\xB7 linux\n", C4P_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--import-skin") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "usage: copy4paste --import-skin FILE.json\n");
                return 2;
            }
            char *dir = c4p_skins_dir();
            char identifier[96];
            char error[256];
            if (!c4p_skin_import(dir, argv[i + 1], identifier, sizeof identifier, error, sizeof error)) {
                fprintf(stderr, "%s\n", error);
                free(dir);
                return 1;
            }
            char *path = c4p_config_path();
            c4p_config imported;
            c4p_config_load(&imported, path);
            snprintf(imported.skin, sizeof imported.skin, "%s", identifier);
            c4p_config_save(&imported, path);
            printf("%s\n", identifier);
            free(path);
            free(dir);
            return 0;
        }
        if (strcmp(argv[i], "--print-support") == 0) {
            /* Used by the tests and by bug reports: what this build can do
             * in this session, including whether another app owns Ctrl+V. */
            const char *runtime = getenv("XDG_RUNTIME_DIR");
            char reply[1024];
            int running = ipc_request("status", reply, sizeof reply);
            int wayland = 0;
            const char *display = getenv("DISPLAY");
            const char *wayland_display = getenv("WAYLAND_DISPLAY");
            if (!display || !*display) wayland = 1;
            else if (wayland_display && *wayland_display) wayland = 1;
            printf("{\"version\":\"%s\",\"platform\":\"linux\",\"session\":\"%s\","
                   "\"running\":%s,\"socket\":\"%s\",\"runtime_dir\":%s,\"status\":\"%s\"}\n",
                   C4P_VERSION, wayland ? "wayland" : "x11", running ? "true" : "false",
                   g_socket_path[0] ? g_socket_path : "", runtime && *runtime ? "true" : "false",
                   running ? reply : "");
            return running ? 0 : 4;
        }
        if (strcmp(argv[i], "--status") == 0) {
            char reply[1024];
            if (!ipc_request("status", reply, sizeof reply)) {
                fprintf(stderr, "copy4paste: no instance is running\n");
                return 4;
            }
            printf("%s\n", reply);
            return 0;
        }
        if (strcmp(argv[i], "--version-json") == 0) {
            c4p_skin_list early;
            c4p_skin_list_init(&early);
            char *early_dir = c4p_skins_dir();
            c4p_skin_list_load(&early, early_dir);
            printf("{\"version\":\"%s\",\"platform\":\"linux\",\"skins\":%d}\n", C4P_VERSION, early.count);
            c4p_skin_list_free(&early);
            free(early_dir);
            return 0;
        }
    }

    /* A Wayland session cannot be served: the compositor neither lets an
     * application read the clipboard in the background nor intercept Ctrl+V.
     * Say it and stop, instead of pretending and showing an empty picker. */
    {
        const char *backend = getenv("GDK_BACKEND");
        const char *wayland_display = getenv("WAYLAND_DISPLAY");
        const char *display = getenv("DISPLAY");
        int will_be_wayland = 0;
        if (backend && strcmp(backend, "wayland") == 0) will_be_wayland = 1;
        else if (!backend && wayland_display && *wayland_display && (!display || !*display))
            will_be_wayland = 1;
        if (will_be_wayland) {
            int wants_help = 0;
            for (i = 1; i < argc; i++) {
                if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "--print-support") == 0) wants_help = 1;
            }
            if (!wants_help) {
                fprintf(stderr,
                        "copy4paste: this is a Wayland session and the app cannot work there.\n"
                        "            Wayland does not let an application read the clipboard in the\n"
                        "            background or intercept Ctrl+V, so there is no history to show.\n"
                        "            Log in to an X11 session (or Xorg) and run copy4paste again.\n"
                        "            Details: https://github.com/KidiNoise/copy4paste/issues\n");
                return 3;
            }
        }
    }

    /* If another copy4paste is running, it owns the history: hand the request
     * over instead of starting a second, empty picker. A plain launch with an
     * instance already there exits quietly instead of adding a duplicate. */
    {
        int wants_select = 0, wants_settings = 0, other_work = self_test || verify_mode;
        for (i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--select") == 0) wants_select = 1;
            else if (strcmp(argv[i], "--settings") == 0) wants_settings = 1;
            else if (strcmp(argv[i], "--skin") == 0 || strcmp(argv[i], "--test-seconds") == 0) i++;
        }
        if (!other_work && (wants_select || wants_settings)) {
            char reply[256];
            const char *command = wants_settings ? "settings" : "select";
            /* The panel opens in the running instance; this process is done.
             * With no instance the request fails and we become the instance. */
            if (ipc_request(command, reply, sizeof reply)) return 0;
        } else if (!other_work) {
            char reply[256];
            if (ipc_request("status", reply, sizeof reply)) {
                fprintf(stderr, "copy4paste: already running (Ctrl+V opens the picker)\n");
                return 0;
            }
        }
    }

    gtk_init(&argc, &argv);

    config_path = c4p_config_path();
    skins_dir = c4p_skins_dir();
    c4p_config_load(&config, config_path);
    c4p_skin_list_init(&skins);
    c4p_skin_list_load(&skins, skins_dir);
    c4p_history_init(&history, config.capacity);

    int select_now = 0;
    int settings_now = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--select") == 0) {
            select_now = 1;
        } else if (strcmp(argv[i], "--settings") == 0) {
            settings_now = 1;
        } else if (strcmp(argv[i], "--skin") == 0 && i + 1 < argc) {
            free(forced_skin);
            forced_skin = g_strdup(argv[++i]);
        } else if (strcmp(argv[i], "--self-test") == 0) {
            self_test = 1;
        } else if (strcmp(argv[i], "--verify") == 0) {
            verify_mode = 1;
        } else if (strcmp(argv[i], "--test-seconds") == 0 && i + 1 < argc) {
            self_test_seconds = atoi(argv[++i]);
            if (self_test_seconds < 1) self_test_seconds = 1;
        }
    }

    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    g_signal_connect(clipboard, "owner-change", G_CALLBACK(on_owner_change), NULL);

    ensure_panel();
    connect_panel_events();
    if (!self_test && !verify_mode) ipc_serve_start();
    setup_hotkeys();

    if (self_test) {
        seed_history();
        show_panel_at_pointer();
        g_timeout_add((guint)self_test_seconds * 1000, finish_self_test, NULL);
    } else if (verify_mode) {
        seed_history();
        show_panel_at_pointer();
#if defined(GDK_WINDOWING_X11)
        g_timeout_add(700, verify_press_digit, NULL);
#endif
        g_timeout_add(2500, finish_verify, NULL);
    } else if (settings_now) {
        open_settings();
    } else if (select_now) {
        show_panel_at_pointer();
    }

    gtk_main();

#if defined(GDK_WINDOWING_X11)
    ungrab_keys();
#endif
    ipc_serve_stop();
    c4p_history_free(&history);
    c4p_skin_list_free(&skins);
    free(config_path);
    free(skins_dir);
    free(forced_skin);
    clear_self_set();
    return 0;
}
