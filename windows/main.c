/* copy4paste for Windows (Win32, no runtime dependencies).
 *
 * Same product as the macOS build: Ctrl+V opens a small picker with your last
 * copies, 1-9/arrows/Enter choose one, Backspace deletes, Esc closes. Skins
 * are the same JSON files, read by the shared core in shared/.
 *
 * The history lives in RAM. Only preferences and imported skins are written:
 * %APPDATA%\copy4paste. No network, no telemetry.
 */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#define C4P_VERSION_TEXT L"1.3"
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "c4p.h"

#define PANEL_WIDE C4P_PANEL_WIDTH
#define ROW_WIDE C4P_ROW_HEIGHT
#define HEADER_WIDE C4P_HEADER_HEIGHT
#define VISIBLE_ROWS C4P_VISIBLE_ROWS

#define WM_C4P_OPEN_PICKER (WM_APP + 10)
#define WM_C4P_OPEN_SETTINGS (WM_APP + 11)
#define WM_C4P_TRAY (WM_APP + 12)
#define ID_TRAY 1
#define ID_TIMER_DISMISS 1
#define ID_TIMER_PASTE 2

#define ID_COPIES 100
#define ID_SKIN 101
#define ID_IMPORT 102
#define ID_OPACITY 103
#define ID_QUIT 104
#define ID_CLOSE 105
#define ID_MENU_PICKER 200
#define ID_MENU_SETTINGS 201
#define ID_MENU_QUIT 202

static HINSTANCE g_instance;
static HWND g_panel;
static HWND g_settings;
static HWND g_message_window;
static HHOOK g_hook;
static HANDLE g_single_mutex;
static HICON g_icon;
static NOTIFYICONDATAW g_tray;
static int g_tray_added;

static c4p_config g_config;
static c4p_skin_list g_skins;
static c4p_history g_history;
static char *g_config_path;
static char *g_skins_dir;

static int g_panel_visible;
static int g_selected;
static int g_first_row;
static int g_displayed_count;
static int g_dpi = 96;
static int g_region_radius = -1;
static int g_panel_opened;
static UINT g_simulate_key; /* test-only: let this injected key through the hook */
static FILE *g_debug;

static char *g_self_set;
static char *g_forced_skin;
static wchar_t g_log_path[MAX_PATH];

static int g_self_test;
static int g_verify;
static int g_test_seconds = 15;

static HFONT g_row_font;
static HFONT g_number_font;
static HFONT g_header_font;
static HFONT g_hint_font;
static HFONT g_gear_font;
static HFONT g_ui_font;
static HFONT g_ui_title_font;
static HFONT g_ui_small_font;
static HBRUSH g_background_brush;
static HBRUSH g_panel_brush;

/* --- small helpers ---------------------------------------------------- */

static int scaled(int value) { return MulDiv(value, g_dpi, 96); }

typedef BOOL(WINAPI *set_dpi_context_fn)(HANDLE);
typedef UINT(WINAPI *get_dpi_for_window_fn)(HWND);
typedef UINT(WINAPI *get_dpi_for_system_fn)(void);

static void enable_dpi_awareness(void) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        set_dpi_context_fn set_context =
            (set_dpi_context_fn)(void *)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (set_context && set_context((HANDLE)(INT_PTR)-4)) return; /* PER_MONITOR_AWARE_V2 */
    }
    SetProcessDPIAware();
}

static UINT system_dpi(void) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        get_dpi_for_system_fn query = (get_dpi_for_system_fn)(void *)GetProcAddress(user32, "GetDpiForSystem");
        if (query) {
            UINT value = query();
            if (value >= 48) return value;
        }
    }
    return 96;
}

static UINT window_dpi(HWND window) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        get_dpi_for_window_fn query =
            (get_dpi_for_window_fn)(void *)GetProcAddress(user32, "GetDpiForWindow");
        if (query) {
            UINT value = query(window);
            if (value >= 48) return value;
        }
    }
    return system_dpi();
}

static wchar_t *widen(const char *utf8) {
    if (!utf8) utf8 = "";
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (needed <= 0) return NULL;
    wchar_t *wide = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)needed);
    if (!wide) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, needed);
    return wide;
}

static char *narrow(const wchar_t *wide) {
    if (!wide) return NULL;
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (needed <= 0) return NULL;
    char *utf8 = (char *)malloc((size_t)needed);
    if (!utf8) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, needed, NULL, NULL);
    return utf8;
}

static void free_fonts(void) {
    if (g_row_font) { DeleteObject(g_row_font); g_row_font = NULL; }
    if (g_number_font) { DeleteObject(g_number_font); g_number_font = NULL; }
    if (g_header_font) { DeleteObject(g_header_font); g_header_font = NULL; }
    if (g_hint_font) { DeleteObject(g_hint_font); g_hint_font = NULL; }
    if (g_gear_font) { DeleteObject(g_gear_font); g_gear_font = NULL; }
    if (g_ui_font) { DeleteObject(g_ui_font); g_ui_font = NULL; }
    if (g_ui_title_font) { DeleteObject(g_ui_title_font); g_ui_title_font = NULL; }
    if (g_ui_small_font) { DeleteObject(g_ui_small_font); g_ui_small_font = NULL; }
    if (g_background_brush) { DeleteObject(g_background_brush); g_background_brush = NULL; }
    if (g_panel_brush) { DeleteObject(g_panel_brush); g_panel_brush = NULL; }
}

static const c4p_skin *current_skin(void) {
    const char *id = g_forced_skin ? g_forced_skin : g_config.skin;
    return c4p_skin_list_resolve(&g_skins, id);
}

static COLORREF to_color(c4p_color color) {
    return RGB((int)(color.r * 255.0 + 0.5), (int)(color.g * 255.0 + 0.5), (int)(color.b * 255.0 + 0.5));
}

/* GDI has no alpha: flatten the selection colour onto the background. */
static COLORREF blend(c4p_color base, c4p_color over) {
    double a = over.a;
    c4p_color mixed;
    mixed.r = base.r * (1.0 - a) + over.r * a;
    mixed.g = base.g * (1.0 - a) + over.g * a;
    mixed.b = base.b * (1.0 - a) + over.b * a;
    return to_color(mixed);
}

static HFONT make_font(const char *face, int logical_points, int weight) {
    wchar_t wide_face[128];
    MultiByteToWideChar(CP_UTF8, 0, face && *face ? face : "Segoe UI", -1, wide_face, 128);
    LOGFONTW font;
    memset(&font, 0, sizeof font);
    font.lfHeight = -MulDiv(logical_points, g_dpi, 72);
    font.lfWeight = weight;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    wcscpy(font.lfFaceName, wide_face);
    return CreateFontIndirectW(&font);
}

static void build_theme_resources(void) {
    const c4p_skin *skin = current_skin();
    free_fonts();
    g_row_font = make_font(skin->font_name, (int)(skin->font_size > 0 ? skin->font_size : 12), FW_NORMAL);
    g_number_font = make_font("Consolas", 11, FW_NORMAL);
    g_header_font = make_font("Segoe UI", 10, FW_NORMAL);
    g_hint_font = make_font("Segoe UI", 9, FW_NORMAL);
    g_gear_font = make_font("Segoe UI Symbol", 13, FW_NORMAL);
    g_ui_font = make_font("Segoe UI", 9, FW_NORMAL);
    g_ui_title_font = make_font("Segoe UI", 11, FW_SEMIBOLD);
    g_ui_small_font = make_font("Segoe UI", 9, FW_NORMAL);
    g_background_brush = CreateSolidBrush(to_color(skin->background));
    g_panel_brush = CreateSolidBrush(to_color(skin->background));
}

/* --- panel geometry --------------------------------------------------- */

static int visible_rows(void) {
    int visible = g_displayed_count < VISIBLE_ROWS ? g_displayed_count : VISIBLE_ROWS;
    return visible < 1 ? 1 : visible;
}

static int panel_height_wide(void) { return HEADER_WIDE + ROW_WIDE * visible_rows(); }

static void clamp_first_row(void) {
    int maximum = g_displayed_count - visible_rows();
    if (maximum < 0) maximum = 0;
    if (g_first_row > maximum) g_first_row = maximum;
    if (g_first_row < 0) g_first_row = 0;
}

static void update_selection(void) {
    if (g_displayed_count <= 0) return;
    if (g_selected < 0) g_selected = 0;
    if (g_selected >= g_displayed_count) g_selected = g_displayed_count - 1;
    int visible = visible_rows();
    if (g_selected < g_first_row) g_first_row = g_selected;
    if (g_selected >= g_first_row + visible) g_first_row = g_selected - visible + 1;
    clamp_first_row();
    if (g_panel) InvalidateRect(g_panel, NULL, FALSE);
}

/* --- clipboard -------------------------------------------------------- */

static void clear_self_set(void) {
    free(g_self_set);
    g_self_set = NULL;
}

static void set_clipboard_text(const char *text) {
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    wchar_t *wide = widen(text && *text ? text : "");
    if (wide) {
        size_t bytes = (wcslen(wide) + 1) * sizeof(wchar_t);
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (handle) {
            void *target = GlobalLock(handle);
            if (target) {
                memcpy(target, wide, bytes);
                GlobalUnlock(handle);
                SetClipboardData(CF_UNICODETEXT, handle);
            } else {
                GlobalFree(handle);
            }
        }
        free(wide);
    }
    CloseClipboard();
    clear_self_set();
    g_self_set = text ? _strdup(text) : NULL;
}

static char *clipboard_text(void) {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return NULL;
    if (!OpenClipboard(NULL)) return NULL;
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    char *text = NULL;
    if (handle) {
        wchar_t *wide = (wchar_t *)GlobalLock(handle);
        if (wide) {
            text = narrow(wide);
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    return text;
}

static int clipboard_has_image_or_files(void) {
    if (IsClipboardFormatAvailable(CF_HDROP)) return 1;
    if (IsClipboardFormatAvailable(CF_DIB)) return 1;
    if (IsClipboardFormatAvailable(CF_BITMAP)) return 1;
    UINT png = RegisterClipboardFormatW(L"PNG");
    if (png && IsClipboardFormatAvailable(png)) return 1;
    return 0;
}

static void poll_clipboard(void) {
    if (clipboard_has_image_or_files()) return;
    char *text = clipboard_text();
    if (!text || !*text) {
        free(text);
        return;
    }
    if (g_self_set && strcmp(text, g_self_set) == 0) {
        free(text);
        return;
    }
    if (strlen(text) > C4P_MAX_TEXT_BYTES) {
        free(text);
        return;
    }
    c4p_history_remember(&g_history, text);
    free(text);
}

/* --- panel painting --------------------------------------------------- */

static void apply_region(void) {
    const c4p_skin *skin = current_skin();
    int radius = (int)(skin->corner_radius * g_dpi / 96.0);
    RECT rect;
    GetWindowRect(g_panel, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (radius == g_region_radius && width > 0) {
        /* Region already matches; nothing to do. */
    }
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius * 2, radius * 2);
    SetWindowRgn(g_panel, region, TRUE);
    g_region_radius = radius;
}

static void draw_gear(HDC dc, int center_x, int center_y, int radius, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, scaled(1), color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, center_x - radius, center_y - radius, center_x + radius + 1, center_y + radius + 1);
    int i;
    for (i = 0; i < 8; i++) {
        double angle = i * 3.14159265358979 / 4.0;
        int inner = radius + scaled(1);
        int outer = radius + scaled(3);
        int x1 = center_x + (int)(cos(angle) * inner);
        int y1 = center_y + (int)(sin(angle) * inner);
        int x2 = center_x + (int)(cos(angle) * outer);
        int y2 = center_y + (int)(sin(angle) * outer);
        MoveToEx(dc, x1, y1, NULL);
        LineTo(dc, x2, y2);
    }
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

static void draw_cross(HDC dc, int center_x, int center_y, COLORREF color) {
    int reach = scaled(4);
    HPEN pen = CreatePen(PS_SOLID, scaled(2), color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    MoveToEx(dc, center_x - reach, center_y - reach, NULL);
    LineTo(dc, center_x + reach + 1, center_y + reach + 1);
    MoveToEx(dc, center_x - reach, center_y + reach + 1, NULL);
    LineTo(dc, center_x + reach + 1, center_y - reach);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

static void paint_panel(HDC target, int width, int height) {
    const c4p_skin *skin = current_skin();
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    HGDIOBJ old_bitmap = SelectObject(dc, bitmap);

    RECT full = { 0, 0, width, height };
    FillRect(dc, &full, g_panel_brush);
    SetBkMode(dc, TRANSPARENT);

    /* Header */
    SelectObject(dc, g_header_font);
    SetTextColor(dc, to_color(skin->muted));
    RECT header = { scaled(10), scaled(4), scaled(140), scaled(22) };
    DrawTextW(dc, L"copy4paste", -1, &header, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    wchar_t hint[64];
    if (g_displayed_count > VISIBLE_ROWS) {
        swprintf(hint, 64, L"%d\x2013%d/%d", g_first_row + 1, g_first_row + visible_rows(), g_displayed_count);
    } else {
        wcscpy(hint, L"1 = latest");
    }
    SelectObject(dc, g_hint_font);
    RECT hint_rect = { width - scaled(46) - scaled(110), scaled(4), width - scaled(46), scaled(22) };
    DrawTextW(dc, hint, -1, &hint_rect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

    draw_gear(dc, width - scaled(21), scaled(12), scaled(4), to_color(skin->muted));

    /* Rows */
    int visible = visible_rows();
    int i;
    for (i = 0; i < visible; i++) {
        int absolute = g_first_row + i;
        if (absolute >= g_displayed_count) break;
        int top = scaled(25) + scaled(ROW_WIDE) * i;

        if (absolute == g_selected) {
            RECT selection = { scaled(6), top + scaled(1), width - scaled(6), top + scaled(ROW_WIDE) - scaled(1) };
            HBRUSH brush = CreateSolidBrush(blend(skin->background, skin->selection));
            HRGN region = CreateRoundRectRgn(selection.left, selection.top, selection.right, selection.bottom,
                                             scaled(10), scaled(10));
            HGDIOBJ old_brush = SelectObject(dc, brush);
            FillRgn(dc, region, brush);
            SelectObject(dc, old_brush);
            DeleteObject(region);
            DeleteObject(brush);
        }

        wchar_t number[16];
        swprintf(number, 16, L"%d", absolute + 1);
        SelectObject(dc, g_number_font);
        SetTextColor(dc, to_color(skin->muted));
        RECT number_rect = { scaled(9), top + scaled(6), scaled(28), top + scaled(26) };
        DrawTextW(dc, number, -1, &number_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        char *preview = c4p_preview_text(g_history.items[absolute]);
        wchar_t *preview_wide = widen(preview ? preview : "");
        free(preview);
        SelectObject(dc, g_row_font);
        SetTextColor(dc, to_color(skin->foreground));
        RECT text_rect = { scaled(29), top + scaled(5), width - scaled(38), top + scaled(27) };
        DrawTextW(dc, preview_wide ? preview_wide : L"", -1, &text_rect,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        free(preview_wide);

        draw_cross(dc, width - scaled(18), top + scaled(15), to_color(skin->delete_color));
    }

    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

static void on_panel_paint(HWND window) {
    PAINTSTRUCT paint;
    HDC dc = BeginPaint(window, &paint);
    RECT client;
    GetClientRect(window, &client);
    paint_panel(dc, client.right - client.left, client.bottom - client.top);
    EndPaint(window, &paint);
}

/* --- panel behaviour -------------------------------------------------- */

static int row_at(int x, int y) {
    int top = scaled(25);
    if (y < top) return -1;
    int local = (y - top) / scaled(ROW_WIDE);
    if (local < 0 || local >= visible_rows()) return -1;
    if (x < scaled(5) || x > scaled(PANEL_WIDE - 5)) return -1;
    int absolute = g_first_row + local;
    if (absolute >= g_displayed_count) return -1;
    return absolute;
}

static int cross_at(int x, int y) {
    int row = row_at(x, y);
    if (row < 0) return -1;
    int local = row - g_first_row;
    int top = scaled(25) + scaled(ROW_WIDE) * local + scaled(4);
    if (x >= scaled(PANEL_WIDE - 32) && y >= top && y <= top + scaled(ROW_WIDE - 8)) return row;
    return -1;
}

static int gear_at(int x, int y) {
    return x >= scaled(PANEL_WIDE - 40) && x <= scaled(PANEL_WIDE - 2) && y >= 0 && y < scaled(24);
}

static void hide_panel(void) {
    if (!g_panel) return;
    KillTimer(g_panel, ID_TIMER_DISMISS);
    ShowWindow(g_panel, SW_HIDE);
    g_panel_visible = 0;
    g_selected = 0;
    g_first_row = 0;
}

static void schedule_paste(void);

static void choose_index(int index) {
    if (!g_panel_visible || index < 0 || index >= g_history.count) {
        hide_panel();
        return;
    }
    char *text = _strdup(g_history.items[index]);
    set_clipboard_text(text);
    free(text);
    hide_panel();
    schedule_paste();
}

static void delete_index(int index) {
    if (index < 0 || index >= g_history.count) return;
    char *text = _strdup(g_history.items[index]);
    char *current = clipboard_text();
    int was_clipboard = current && strcmp(current, text) == 0;
    free(current);

    c4p_history_remove(&g_history, text);
    if (was_clipboard) {
        if (g_history.count > 0) set_clipboard_text(g_history.items[0]);
        else {
            if (OpenClipboard(NULL)) {
                EmptyClipboard();
                CloseClipboard();
            }
            clear_self_set();
        }
    }
    free(text);
    g_displayed_count = g_history.count;
    if (g_history.count < 2) {
        hide_panel();
        return;
    }
    if (g_selected >= g_displayed_count) g_selected = g_displayed_count - 1;
    update_selection();
    RECT rect;
    GetWindowRect(g_panel, &rect);
    SetWindowPos(g_panel, HWND_TOPMOST, rect.left, rect.bottom - scaled(panel_height_wide()),
                 scaled(PANEL_WIDE), scaled(panel_height_wide()), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    apply_region();
    InvalidateRect(g_panel, NULL, FALSE);
    SetTimer(g_panel, ID_TIMER_DISMISS, 12000, NULL);
}

static void show_panel_at_pointer(void) {
    if (g_history.count < 2) return;
    if (!g_panel) return;
    g_selected = 0;
    g_first_row = 0;
    g_displayed_count = g_history.count;
    g_panel_visible = 1;
    g_panel_opened = 1;

    POINT cursor;
    GetCursorPos(&cursor);
    MONITORINFO monitor;
    monitor.cbSize = sizeof monitor;
    HMONITOR handle = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfoW(handle, &monitor);
    RECT work = monitor.rcWork;

    int width = scaled(PANEL_WIDE);
    int height = scaled(panel_height_wide());
    int x = cursor.x + scaled(9);
    int y = cursor.y - height + scaled(8);
    if (x > work.right - width - scaled(6)) x = work.right - width - scaled(6);
    if (x < work.left + scaled(6)) x = work.left + scaled(6);
    if (y > work.bottom - height - scaled(6)) y = work.bottom - height - scaled(6);
    if (y < work.top + scaled(6)) y = work.top + scaled(6);

    SetWindowPos(g_panel, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    apply_region();
    InvalidateRect(g_panel, NULL, FALSE);
    SetTimer(g_panel, ID_TIMER_DISMISS, 12000, NULL);
}

static void send_paste(void) {
    INPUT inputs[4];
    memset(inputs, 0, sizeof inputs);
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
}

static void schedule_paste(void) {
    if (g_panel) SetTimer(g_panel, ID_TIMER_PASTE, 150, NULL);
}

/* --- settings --------------------------------------------------------- */

static void rebuild_settings(void) {
    if (g_settings) {
        DestroyWindow(g_settings);
        g_settings = NULL;
    }
    PostMessageW(g_message_window, WM_C4P_OPEN_SETTINGS, 0, 0);
}

static void fill_skin_combo(HWND window) {
    HWND combo = GetDlgItem(window, ID_SKIN);
    if (!combo) return;
    const c4p_skin *skin = current_skin();
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    int i;
    for (i = 0; i < g_skins.count; i++) {
        wchar_t *name = widen(g_skins.items[i].name);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)name);
        free(name);
        if (strcmp(g_skins.items[i].id, skin->id) == 0) {
            SendMessageW(combo, CB_SETCURSEL, (WPARAM)i, 0);
        }
    }
}

static void import_skin(HWND window) {
    wchar_t file[MAX_PATH];
    file[0] = L'\0';
    OPENFILENAMEW dialog;
    memset(&dialog, 0, sizeof dialog);
    dialog.lStructSize = sizeof dialog;
    dialog.hwndOwner = window;
    dialog.lpstrFile = file;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter = L"copy4paste skins (*.json)\0*.json\0All files\0*.*\0";
    dialog.lpstrTitle = L"Choose a copy4paste skin";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&dialog)) return;

    char *path = narrow(file);
    char identifier[96];
    char error[256];
    int ok = c4p_skin_import(g_skins_dir, path, identifier, sizeof identifier, error, sizeof error);
    free(path);
    if (!ok) {
        wchar_t *message = widen(error);
        MessageBoxW(window, message ? message : L"The skin could not be imported.",
                    L"Could not import skin", MB_OK | MB_ICONWARNING);
        free(message);
        return;
    }
    c4p_skin_list_load(&g_skins, g_skins_dir);
    snprintf(g_config.skin, sizeof g_config.skin, "%s", identifier);
    c4p_config_save(&g_config, g_config_path);
    build_theme_resources();
    rebuild_settings();
}

static void create_settings(HWND owner) {
    if (g_settings) {
        SetForegroundWindow(g_settings);
        return;
    }
    const c4p_skin *skin = current_skin();
    int width = scaled(360);
    int height = scaled(324);

    wchar_t title_text[128];
    wcscpy(title_text, L"copy4paste \x00B7 Settings \x00B7 " C4P_VERSION_TEXT);
    g_settings = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"Copy4pasteSettings", title_text,
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_POPUP, CW_USEDEFAULT,
                                 CW_USEDEFAULT, width, height, NULL, NULL, g_instance, NULL);
    if (!g_settings) return;

    POINT cursor;
    GetCursorPos(&cursor);
    MONITORINFO monitor;
    monitor.cbSize = sizeof monitor;
    GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitor);
    int x = cursor.x - width / 2;
    int y = cursor.y - height / 2;
    if (x < monitor.rcWork.left + scaled(8)) x = monitor.rcWork.left + scaled(8);
    if (y < monitor.rcWork.top + scaled(8)) y = monitor.rcWork.top + scaled(8);
    SetWindowPos(g_settings, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);

    HWND title = CreateWindowExW(0, L"STATIC", title_text,
                                 WS_CHILD | WS_VISIBLE, scaled(16), scaled(14), scaled(320), scaled(20),
                                 g_settings, NULL, g_instance, NULL);
    SendMessageW(title, WM_SETFONT, (WPARAM)g_ui_title_font, TRUE);

    HWND copies_label = CreateWindowExW(0, L"STATIC", L"Copies", WS_CHILD | WS_VISIBLE, scaled(16),
                                        scaled(56), scaled(120), scaled(20), g_settings, NULL, g_instance, NULL);
    SendMessageW(copies_label, WM_SETFONT, (WPARAM)g_ui_font, TRUE);
    HWND copies = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_RIGHT,
                                  scaled(200), scaled(52), scaled(80), scaled(24), g_settings,
                                  (HMENU)(INT_PTR)ID_COPIES, g_instance, NULL);
    SendMessageW(copies, WM_SETFONT, (WPARAM)g_ui_font, TRUE);
    wchar_t buffer[16];
    swprintf(buffer, 16, L"%d", g_config.capacity);
    SetWindowTextW(copies, buffer);
    HWND spinner = CreateWindowExW(0, UPDOWN_CLASSW, NULL,
                                   WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS,
                                   scaled(280), scaled(52), scaled(20), scaled(24), g_settings, NULL, g_instance,
                                   NULL);
    SendMessageW(spinner, UDM_SETBUDDY, (WPARAM)copies, 0);
    SendMessageW(spinner, UDM_SETRANGE32, C4P_CAPACITY_MIN, C4P_CAPACITY_MAX);
    SendMessageW(spinner, UDM_SETPOS32, 0, g_config.capacity);

    HWND skin_label = CreateWindowExW(0, L"STATIC", L"Skin", WS_CHILD | WS_VISIBLE, scaled(16), scaled(96),
                                      scaled(120), scaled(20), g_settings, NULL, g_instance, NULL);
    SendMessageW(skin_label, WM_SETFONT, (WPARAM)g_ui_font, TRUE);
    HWND combo = CreateWindowExW(0, L"COMBOBOX", NULL,
                                 WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, scaled(200), scaled(92),
                                 scaled(140), scaled(200), g_settings, (HMENU)(INT_PTR)ID_SKIN, g_instance, NULL);
    SendMessageW(combo, WM_SETFONT, (WPARAM)g_ui_font, TRUE);

    HWND import_button = CreateWindowExW(0, L"BUTTON", L"Import JSON skin\x2026",
                                         WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, scaled(200), scaled(126),
                                         scaled(140), scaled(26), g_settings, (HMENU)(INT_PTR)ID_IMPORT,
                                         g_instance, NULL);
    SendMessageW(import_button, WM_SETFONT, (WPARAM)g_ui_font, TRUE);

    HWND opacity_label = CreateWindowExW(0, L"STATIC", L"Opacity", WS_CHILD | WS_VISIBLE, scaled(16),
                                         scaled(166), scaled(120), scaled(20), g_settings, NULL, g_instance,
                                         NULL);
    SendMessageW(opacity_label, WM_SETFONT, (WPARAM)g_ui_font, TRUE);
    HWND opacity = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_RIGHT,
                                   scaled(200), scaled(162), scaled(60), scaled(24), g_settings,
                                   (HMENU)(INT_PTR)ID_OPACITY, g_instance, NULL);
    SendMessageW(opacity, WM_SETFONT, (WPARAM)g_ui_font, TRUE);
    swprintf(buffer, 16, L"%d", g_config.opacity);
    SetWindowTextW(opacity, buffer);
    HWND percent = CreateWindowExW(0, L"STATIC", L"%", WS_CHILD | WS_VISIBLE, scaled(264), scaled(166),
                                   scaled(20), scaled(20), g_settings, NULL, g_instance, NULL);
    SendMessageW(percent, WM_SETFONT, (WPARAM)g_ui_font, TRUE);

    HWND help = CreateWindowExW(0, L"STATIC",
                                L"1\x2013" L"30 copies \x00B7 RAM only\nSkins: %APPDATA%\\copy4paste\\skins",
                                WS_CHILD | WS_VISIBLE, scaled(16), scaled(196), scaled(332), scaled(36),
                                g_settings, NULL, g_instance, NULL);
    SendMessageW(help, WM_SETFONT, (WPARAM)g_ui_small_font, TRUE);

    HWND quit = CreateWindowExW(0, L"BUTTON", L"Quit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, scaled(190),
                                scaled(252), scaled(70), scaled(28), g_settings, (HMENU)(INT_PTR)ID_QUIT,
                                g_instance, NULL);
    SendMessageW(quit, WM_SETFONT, (WPARAM)g_ui_font, TRUE);
    HWND close = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, scaled(268),
                                 scaled(252), scaled(72), scaled(28), g_settings, (HMENU)(INT_PTR)ID_CLOSE,
                                 g_instance, NULL);
    SendMessageW(close, WM_SETFONT, (WPARAM)g_ui_font, TRUE);

    fill_skin_combo(g_settings);
    (void)skin;
    (void)owner;
    ShowWindow(g_settings, SW_SHOWNOACTIVATE);
    SetForegroundWindow(g_settings);
}

static void read_settings_controls(HWND window) {
    wchar_t buffer[32];
    HWND copies = GetDlgItem(window, ID_COPIES);
    if (copies && GetWindowTextW(copies, buffer, 32)) {
        int value = _wtoi(buffer);
        if (value >= C4P_CAPACITY_MIN && value <= C4P_CAPACITY_MAX && value != g_config.capacity) {
            g_config.capacity = value;
            c4p_history_set_capacity(&g_history, value);
            c4p_config_save(&g_config, g_config_path);
            if (g_panel_visible) {
                g_displayed_count = g_history.count;
                if (g_history.count < 2) hide_panel();
                else {
                    update_selection();
                    InvalidateRect(g_panel, NULL, FALSE);
                }
            }
        }
    }
    HWND opacity = GetDlgItem(window, ID_OPACITY);
    if (opacity && GetWindowTextW(opacity, buffer, 32)) {
        int value = _wtoi(buffer);
        if (value >= C4P_OPACITY_MIN && value <= C4P_OPACITY_MAX && value != g_config.opacity) {
            g_config.opacity = value;
            c4p_config_save(&g_config, g_config_path);
            if (g_panel) {
                SetLayeredWindowAttributes(g_panel, 0, (BYTE)((g_config.opacity * 255) / 100), LWA_ALPHA);
            }
        }
    }
}

/* --- window procedures ------------------------------------------------ */

static LRESULT CALLBACK panel_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_PAINT:
            on_panel_paint(window);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lparam);
            int y = GET_Y_LPARAM(lparam);
            int row = row_at(x, y);
            if (row >= 0 && row != g_selected) {
                g_selected = row;
                InvalidateRect(window, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lparam);
            int y = GET_Y_LPARAM(lparam);
            if (gear_at(x, y)) {
                SendMessageW(g_message_window, WM_C4P_OPEN_SETTINGS, 0, 0);
                return 0;
            }
            int cross = cross_at(x, y);
            if (cross >= 0) {
                delete_index(cross);
                return 0;
            }
            int row = row_at(x, y);
            if (row >= 0) {
                choose_index(row);
                return 0;
            }
            hide_panel();
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wparam);
            if (delta == 0 || g_displayed_count <= VISIBLE_ROWS) return 0;
            int step = delta < 0 ? 1 : -1;
            int maximum = g_displayed_count - VISIBLE_ROWS;
            int next = g_first_row + step;
            if (next < 0) next = 0;
            if (next > maximum) next = maximum;
            if (next != g_first_row) {
                g_first_row = next;
                g_selected = next;
                InvalidateRect(window, NULL, FALSE);
                SetTimer(window, ID_TIMER_DISMISS, 12000, NULL);
            }
            return 0;
        }
        case WM_TIMER:
            if (wparam == ID_TIMER_DISMISS) {
                KillTimer(window, ID_TIMER_DISMISS);
                hide_panel();
            } else if (wparam == ID_TIMER_PASTE) {
                KillTimer(window, ID_TIMER_PASTE);
                send_paste();
            }
            return 0;
        case WM_DPICHANGED: {
            g_dpi = HIWORD(wparam);
            build_theme_resources();
            RECT *suggested = (RECT *)lparam;
            SetWindowPos(window, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            apply_region();
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static void settings_command(HWND window, int control) {
    switch (control) {
        case ID_IMPORT:
            import_skin(window);
            break;
        case ID_QUIT:
            c4p_config_save(&g_config, g_config_path);
            DestroyWindow(window);
            PostQuitMessage(0);
            break;
        case ID_CLOSE:
            read_settings_controls(window);
            DestroyWindow(window);
            PostMessageW(g_message_window, WM_C4P_OPEN_PICKER, 0, 0);
            break;
        default:
            break;
    }
}

static LRESULT CALLBACK settings_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_COMMAND: {
            int control = LOWORD(wparam);
            int code = HIWORD(wparam);
            if (control == ID_SKIN && code == CBN_SELCHANGE) {
                int index = (int)SendMessageW((HWND)lparam, CB_GETCURSEL, 0, 0);
                if (index >= 0 && index < g_skins.count) {
                    snprintf(g_config.skin, sizeof g_config.skin, "%s", g_skins.items[index].id);
                    free(g_forced_skin);
                    g_forced_skin = NULL;
                    c4p_config_save(&g_config, g_config_path);
                    build_theme_resources();
                    g_region_radius = -1;
                    if (g_panel) {
                        apply_region();
                        SetLayeredWindowAttributes(g_panel, 0, (BYTE)((g_config.opacity * 255) / 100),
                                                   LWA_ALPHA);
                        InvalidateRect(g_panel, NULL, FALSE);
                    }
                    rebuild_settings();
                }
                return 0;
            }
            if (control == ID_CLOSE && code == BN_CLICKED) {
                settings_command(window, ID_CLOSE);
                return 0;
            }
            if (control == ID_IMPORT && code == BN_CLICKED) {
                import_skin(window);
                return 0;
            }
            if (control == ID_QUIT && code == BN_CLICKED) {
                settings_command(window, ID_QUIT);
                return 0;
            }
            if ((control == ID_COPIES || control == ID_OPACITY) && code == EN_CHANGE) {
                read_settings_controls(window);
                return 0;
            }
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            const c4p_skin *skin = current_skin();
            HDC dc = (HDC)wparam;
            SetTextColor(dc, to_color(skin->foreground));
            SetBkColor(dc, to_color(skin->background));
            return (LRESULT)g_background_brush;
        }
        case WM_CTLCOLORBTN: {
            const c4p_skin *skin = current_skin();
            HDC dc = (HDC)wparam;
            SetTextColor(dc, to_color(skin->foreground));
            SetBkColor(dc, to_color(skin->background));
            return (LRESULT)g_background_brush;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            const c4p_skin *skin = current_skin();
            HDC dc = (HDC)wparam;
            SetTextColor(dc, to_color(skin->foreground));
            SetBkColor(dc, to_color(skin->background));
            return (LRESULT)g_background_brush;
        }
        case WM_ERASEBKGND: {
            RECT client;
            GetClientRect(window, &client);
            FillRect((HDC)wparam, &client, g_background_brush);
            return 1;
        }
        case WM_CLOSE:
            read_settings_controls(window);
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            g_settings = NULL;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static void show_tray_menu(HWND window) {
    POINT cursor;
    GetCursorPos(&cursor);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_MENU_PICKER, L"Open picker");
    AppendMenuW(menu, MF_STRING, ID_MENU_SETTINGS, L"Settings\x2026");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_MENU_QUIT, L"Quit copy4paste");
    SetForegroundWindow(window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, window, NULL);
    DestroyMenu(menu);
}

static LRESULT CALLBACK message_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CLIPBOARDUPDATE:
            poll_clipboard();
            return 0;
        case WM_TIMER:
            /* Safety net: WM_CLIPBOARDUPDATE is the primary signal, but some
             * clipboard owners (and Wine's X11 bridge) only change the
             * clipboard without notifying. 400 ms, like the macOS poll. */
            if (wparam == 20) poll_clipboard();
            return 0;
        case WM_C4P_OPEN_PICKER:
            poll_clipboard();
            show_panel_at_pointer();
            return 0;
        case WM_C4P_OPEN_SETTINGS:
            create_settings(g_panel);
            return 0;
        case WM_C4P_TRAY:
            if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU) {
                show_tray_menu(window);
            } else if (lparam == WM_LBUTTONDBLCLK) {
                poll_clipboard();
                show_panel_at_pointer();
            }
            return 0;
        case WM_COMMAND:
            if (LOWORD(wparam) == ID_MENU_PICKER) {
                poll_clipboard();
                show_panel_at_pointer();
            } else if (LOWORD(wparam) == ID_MENU_SETTINGS) {
                create_settings(g_panel);
            } else if (LOWORD(wparam) == ID_MENU_QUIT) {
                c4p_config_save(&g_config, g_config_path);
                DestroyWindow(window);
                PostQuitMessage(0);
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

/* --- keyboard hook ---------------------------------------------------- */

static void handle_key_in_panel(UINT key) {
    if (key >= '1' && key <= '9') {
        int index = (int)(key - '1');
        if (index < g_displayed_count) choose_index(index);
        return;
    }
    if (key >= VK_NUMPAD1 && key <= VK_NUMPAD9) {
        int index = (int)(key - VK_NUMPAD1);
        if (index < g_displayed_count) choose_index(index);
        return;
    }
    switch (key) {
        case VK_ESCAPE:
            hide_panel();
            break;
        case VK_DOWN:
            g_selected = (g_selected + 1) % g_displayed_count;
            update_selection();
            SetTimer(g_panel, ID_TIMER_DISMISS, 12000, NULL);
            break;
        case VK_UP:
            g_selected = (g_selected - 1 + g_displayed_count) % g_displayed_count;
            update_selection();
            SetTimer(g_panel, ID_TIMER_DISMISS, 12000, NULL);
            break;
        case VK_RETURN:
            choose_index(g_selected);
            break;
        case VK_BACK:
        case VK_DELETE:
            delete_index(g_selected);
            break;
        default:
            break;
    }
}

static LRESULT CALLBACK keyboard_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT *info = (KBDLLHOOKSTRUCT *)lparam;
        int injected = (info->flags & LLKHF_INJECTED) != 0;
        if (injected && !(g_simulate_key != 0 && info->vkCode == g_simulate_key)) {
            return CallNextHookEx(g_hook, code, wparam, lparam);
        }
        int ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        if (g_debug) {
            fprintf(g_debug, "hook vk=%lu ctrl=%d alt=%d shift=%d injected=%d\n", (unsigned long)info->vkCode,
                    ctrl, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0,
                    (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0, injected);
            fflush(g_debug);
        }
        int alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        int shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

        if (ctrl && alt && !shift && info->vkCode == 'P') {
            SendMessageW(g_message_window, WM_C4P_OPEN_SETTINGS, 0, 0);
            return 1;
        }
        if (!g_panel_visible && ctrl && !alt && !shift && info->vkCode == 'V') {
            poll_clipboard();
            if (g_history.count > 1) {
                show_panel_at_pointer();
                return 1;
            }
            return CallNextHookEx(g_hook, code, wparam, lparam);
        }
        if (g_panel_visible) {
            handle_key_in_panel(info->vkCode);
            return 1;
        }
    }
    return CallNextHookEx(g_hook, code, wparam, lparam);
}

/* --- logging and tests ------------------------------------------------ */

static void log_line(FILE *file, const char *key, const char *value) {
    if (file) fprintf(file, "%s=%s\n", key, value ? value : "");
}

static FILE *open_log(void) {
    if (g_log_path[0]) {
        return _wfopen(g_log_path, L"w");
    }
    wchar_t temp[MAX_PATH];
    DWORD length = GetTempPathW(MAX_PATH, temp);
    if (length == 0) return NULL;
    wcscat(temp, L"copy4paste-verify.log");
    return _wfopen(temp, L"w");
}

static void seed_history(void) {
    c4p_history_remember(&g_history, "First sample copy");
    c4p_history_remember(&g_history, "Second sample copy");
    c4p_history_remember(&g_history, "Third sample copy with a much longer line that will be cut off");
    c4p_history_remember(&g_history, "Fourth sample copy");
}

static void finish_self_test(void) {
    FILE *file = open_log();
    const c4p_skin *skin = current_skin();
    if (file) {
        char value[128];
        log_line(file, "VERSION", C4P_VERSION);
        snprintf(value, sizeof value, "%d", g_skins.count);
        log_line(file, "SKINS", value);
        log_line(file, "SKIN", skin->id);
        log_line(file, "SKIN_NAME", skin->name);
        log_line(file, "LIGHT", skin->light ? "1" : "0");
        snprintf(value, sizeof value, "%.1f", skin->corner_radius);
        log_line(file, "RADIUS", value);
        snprintf(value, sizeof value, "%d", g_history.count);
        log_line(file, "ENTRIES", value);
        log_line(file, "PANEL_VISIBLE", g_panel_visible ? "1" : "0");
        log_line(file, "PANEL_WIDTH", "290");
        snprintf(value, sizeof value, "%d", panel_height_wide());
        log_line(file, "PANEL_HEIGHT", value);
        snprintf(value, sizeof value, "%d", g_config.opacity);
        log_line(file, "OPACITY", value);
        snprintf(value, sizeof value, "%d", g_config.capacity);
        log_line(file, "CAPACITY", value);
        log_line(file, "FIRST", g_history.count > 0 ? g_history.items[0] : "");
        fclose(file);
    }
    PostQuitMessage(0);
}

static DWORD WINAPI verify_thread(LPVOID unused) {
    (void)unused;
    Sleep(600);
    /* First Ctrl+V: the real trigger, through SendInput and the low-level hook.
     * Only the V key is let through the "ignore injected keys" guard. */
    g_simulate_key = 'V';
    INPUT chord[4];
    memset(chord, 0, sizeof chord);
    chord[0].type = INPUT_KEYBOARD;
    chord[0].ki.wVk = VK_CONTROL;
    chord[1].type = INPUT_KEYBOARD;
    chord[1].ki.wVk = 'V';
    chord[2].type = INPUT_KEYBOARD;
    chord[2].ki.wVk = 'V';
    chord[2].ki.dwFlags = KEYEVENTF_KEYUP;
    chord[3].type = INPUT_KEYBOARD;
    chord[3].ki.wVk = VK_CONTROL;
    chord[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, chord, sizeof(INPUT));
    Sleep(600);
    if (g_debug) {
        fprintf(g_debug, "panel after ctrl+v: %d, entries=%d\n", g_panel_visible, g_history.count);
        fflush(g_debug);
    }
    g_simulate_key = '2';
    INPUT digit[2];
    memset(digit, 0, sizeof digit);
    digit[0].type = INPUT_KEYBOARD;
    digit[0].ki.wVk = '2';
    digit[1].type = INPUT_KEYBOARD;
    digit[1].ki.wVk = '2';
    digit[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, digit, sizeof(INPUT));
    Sleep(500);
    g_simulate_key = 0;
    Sleep(900);
    PostMessageW(g_message_window, WM_APP + 31, 0, 0);
    return 0;
}

static void finish_verify(void) {
    char *text = clipboard_text();
    int ok = text && g_history.count > 1 && strcmp(text, g_history.items[1]) == 0 && !g_panel_visible &&
             g_panel_opened;
    FILE *file = open_log();
    if (file) {
        log_line(file, "VERSION", C4P_VERSION);
        char value[128];
        snprintf(value, sizeof value, "%d", g_history.count);
        log_line(file, "ENTRIES", value);
        log_line(file, "PANEL_VISIBLE_AFTER_DIGIT", g_panel_visible ? "1" : "0");
        log_line(file, "PANEL_OPENED_BY_CTRL_V", g_panel_opened ? "1" : "0");
        log_line(file, "CLIPBOARD", text ? text : "");
        log_line(file, "RESULT", ok ? "PASS" : "FAIL");
        fclose(file);
    } else {
        OutputDebugStringA(ok ? "copy4paste verify PASS\n" : "copy4paste verify FAIL\n");
    }
    free(text);
    PostQuitMessage(ok ? 0 : 1);
}

/* --- startup ---------------------------------------------------------- */

static void keep_single_instance(void) {
    g_single_mutex = CreateMutexW(NULL, TRUE, L"Local\\copy4paste-single-instance");
    if (g_single_mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"Copy4pasteMessage", NULL);
        if (existing) {
            PostMessageW(existing, WM_C4P_OPEN_PICKER, 0, 0);
        }
        ExitProcess(0);
    }
}

static void install_tray(HWND window) {
    memset(&g_tray, 0, sizeof g_tray);
    g_tray.cbSize = sizeof g_tray;
    g_tray.hWnd = window;
    g_tray.uID = ID_TRAY;
    g_tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_tray.uCallbackMessage = WM_C4P_TRAY;
    g_tray.hIcon = g_icon ? g_icon : LoadIconW(NULL, IDI_APPLICATION);
    wcscpy(g_tray.szTip, L"copy4paste \x2014 Ctrl+V to choose a copy");
    g_tray_added = Shell_NotifyIconW(NIM_ADD, &g_tray) ? 1 : 0;
}

static int register_classes(void) {
    WNDCLASSEXW panel_class;
    memset(&panel_class, 0, sizeof panel_class);
    panel_class.cbSize = sizeof panel_class;
    panel_class.lpfnWndProc = panel_proc;
    panel_class.hInstance = g_instance;
    panel_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    panel_class.lpszClassName = L"Copy4pastePanel";
    if (!RegisterClassExW(&panel_class)) return 0;

    WNDCLASSEXW settings_class;
    memset(&settings_class, 0, sizeof settings_class);
    settings_class.cbSize = sizeof settings_class;
    settings_class.lpfnWndProc = settings_proc;
    settings_class.hInstance = g_instance;
    settings_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    settings_class.lpszClassName = L"Copy4pasteSettings";
    if (!RegisterClassExW(&settings_class)) return 0;

    WNDCLASSEXW message_class;
    memset(&message_class, 0, sizeof message_class);
    message_class.cbSize = sizeof message_class;
    message_class.lpfnWndProc = message_proc;
    message_class.hInstance = g_instance;
    message_class.lpszClassName = L"Copy4pasteMessage";
    if (!RegisterClassExW(&message_class)) return 0;
    return 1;
}

static int parse_arguments(int argc, wchar_t **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"--version") == 0) {
            char line[64];
            snprintf(line, sizeof line, "copy4paste %s \xC2\xB7 windows\n", C4P_VERSION);
            HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD written = 0;
            if (out && out != INVALID_HANDLE_VALUE) WriteFile(out, line, (DWORD)strlen(line), &written, NULL);
            return 1;
        }
        if (wcscmp(argv[i], L"--self-test") == 0) g_self_test = 1;
        else if (wcscmp(argv[i], L"--verify") == 0) g_verify = 1;
        else if (wcscmp(argv[i], L"--skin") == 0 && i + 1 < argc) {
            free(g_forced_skin);
            g_forced_skin = narrow(argv[++i]);
        } else if (wcscmp(argv[i], L"--log") == 0 && i + 1 < argc) {
            wcsncpy(g_log_path, argv[++i], MAX_PATH - 1);
        } else if (wcscmp(argv[i], L"--test-seconds") == 0 && i + 1 < argc) {
            g_test_seconds = _wtoi(argv[++i]);
            if (g_test_seconds < 1) g_test_seconds = 1;
        }
    }
    return 0;
}

static int wants_command(int argc, wchar_t **argv, const wchar_t *name) {
    int i;
    for (i = 1; i < argc; i++) {
        if (wcscmp(argv[i], name) == 0) return 1;
    }
    return 0;
}

static void import_skin_cli(int argc, wchar_t **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"--import-skin") == 0 && i + 1 < argc) {
            char *path = narrow(argv[i + 1]);
            char identifier[96];
            char error[256];
            if (c4p_skin_import(g_skins_dir, path, identifier, sizeof identifier, error, sizeof error)) {
                FILE *file = open_log();
                if (file) {
                    log_line(file, "IMPORTED", identifier);
                    fclose(file);
                }
                ExitProcess(0);
            }
            FILE *file = open_log();
            if (file) {
                log_line(file, "ERROR", error);
                fclose(file);
            }
            ExitProcess(1);
        }
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show) {
    (void)previous;
    (void)command_line;
    (void)show;
    g_instance = instance;
    enable_dpi_awareness();

    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    g_config_path = c4p_config_path();
    g_skins_dir = c4p_skins_dir();
    c4p_config_load(&g_config, g_config_path);
    c4p_skin_list_init(&g_skins);
    c4p_skin_list_load(&g_skins, g_skins_dir);
    c4p_history_init(&g_history, g_config.capacity);

    if (argc > 1 && parse_arguments(argc, argv)) {
        return 0;
    }
    if (argc > 1 && wants_command(argc, argv, L"--import-skin")) {
        import_skin_cli(argc, argv);
    }

    if (!g_self_test && !g_verify && !wants_command(argc, argv, L"--settings") &&
        !wants_command(argc, argv, L"--select")) {
        keep_single_instance();
    }

    INITCOMMONCONTROLSEX controls;
    controls.dwSize = sizeof controls;
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&controls);

    g_dpi = (int)system_dpi();
    build_theme_resources();

    if (!register_classes()) return 1;

    g_panel = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                              L"Copy4pastePanel", L"copy4paste", WS_POPUP, 0, 0, scaled(PANEL_WIDE),
                              scaled(HEADER_WIDE + ROW_WIDE * VISIBLE_ROWS), NULL, NULL, instance, NULL);
    if (!g_panel) return 1;
    SetLayeredWindowAttributes(g_panel, 0, (BYTE)((g_config.opacity * 255) / 100), LWA_ALPHA);

    g_message_window = CreateWindowExW(0, L"Copy4pasteMessage", L"copy4paste", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                       NULL, instance, NULL);
    if (!g_message_window) return 1;

    AddClipboardFormatListener(g_message_window);
    SetTimer(g_message_window, 20, 400, NULL);
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_proc, instance, 0);
    if (getenv("C4P_DEBUG")) {
        g_debug = fopen("C:\\c4p-debug.log", "w");
        if (g_debug) {
            fprintf(g_debug, "hook=%p error=%lu\n", (void *)g_hook, (unsigned long)GetLastError());
            fflush(g_debug);
        }
    }

    g_icon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    install_tray(g_message_window);

    g_dpi = (int)window_dpi(g_panel);
    build_theme_resources();

    if (g_self_test) {
        seed_history();
        g_displayed_count = g_history.count;
        show_panel_at_pointer();
        SetTimer(g_message_window, 10, (UINT)g_test_seconds * 1000, NULL);
    } else if (g_verify) {
        seed_history();
        g_displayed_count = g_history.count;
        g_panel_opened = 0;
        HANDLE thread = CreateThread(NULL, 0, verify_thread, NULL, 0, NULL);
        if (thread) CloseHandle(thread);
    } else if (wants_command(argc, argv, L"--settings")) {
        create_settings(NULL);
    } else if (wants_command(argc, argv, L"--select")) {
        poll_clipboard();
        show_panel_at_pointer();
    } else {
        poll_clipboard();
    }

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0)) {
        if (message.message == WM_APP + 31) {
            finish_verify();
            break;
        }
        if (message.message == WM_TIMER && message.hwnd == g_message_window && message.wParam == 10) {
            KillTimer(g_message_window, 10);
            finish_self_test();
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_hook) UnhookWindowsHookEx(g_hook);
    if (g_tray_added) Shell_NotifyIconW(NIM_DELETE, &g_tray);
    c4p_config_save(&g_config, g_config_path);
    c4p_history_free(&g_history);
    c4p_skin_list_free(&g_skins);
    free(g_config_path);
    free(g_skins_dir);
    free(g_forced_skin);
    clear_self_set();
    free_fonts();
    return 0;
}
