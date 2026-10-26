/* Test helper: an independent Windows application that receives a paste.
 *
 * It is deliberately NOT part of copy4paste: a plain Win32 window with an edit
 * control, the standard WM_PASTE path, in its own process. The point of the
 * test is to see the text arrive in another application's control, not only on
 * the clipboard.
 *
 * Usage: paste_target.exe <log-file> <seconds>
 * The control text is written to the log file on every change, then the
 * process exits after <seconds>.
 */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

static wchar_t *g_log_path;
static HWND g_edit;
static wchar_t g_last[4096];

static void write_log(const wchar_t *text) {
    FILE *file = _wfopen(g_log_path, L"w");
    if (!file) return;
    char utf8[8192];
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof utf8, NULL, NULL);
    if (length > 0) fputs(utf8, file);
    fclose(file);
}

static LRESULT CALLBACK target_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_TIMER: {
            wchar_t buffer[4096];
            GetWindowTextW(g_edit, buffer, 4096);
            if (wcscmp(buffer, g_last) != 0) {
                wcscpy(g_last, buffer);
                write_log(buffer);
            }
            /* Keep the caret on the control: the paste must land where the
             * focus is, like in any other application. */
            if (GetFocus() != g_edit) SetFocus(g_edit);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show) {
    (void)previous;
    (void)command_line;
    (void)show;
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc < 3) {
        MessageBoxW(NULL, L"usage: paste_target.exe <log-file> <seconds>", L"paste_target", MB_OK);
        return 2;
    }
    g_log_path = _wcsdup(argv[1]);
    int seconds = _wtoi(argv[2]);
    if (seconds <= 0) seconds = 20;
    write_log(L"");

    WNDCLASSEXW window_class;
    memset(&window_class, 0, sizeof window_class);
    window_class.cbSize = sizeof window_class;
    window_class.lpfnWndProc = target_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"Copy4pasteTestTarget";
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    if (!RegisterClassExW(&window_class)) return 1;

    HWND window = CreateWindowExW(WS_EX_TOPMOST, L"Copy4pasteTestTarget", L"Paste target",
                                  WS_OVERLAPPEDWINDOW, 80, 80, 520, 220, NULL, NULL, instance, NULL);
    if (!window) return 1;
    g_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, 10, 10, 480,
                             160, window, (HMENU)1, instance, NULL);
    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);
    SetFocus(g_edit);
    SetTimer(window, 1, 100, NULL);
    SetTimer(window, 2, (UINT)seconds * 1000, NULL);

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0)) {
        if (message.message == WM_TIMER && message.hwnd == window && message.wParam == 2) {
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
