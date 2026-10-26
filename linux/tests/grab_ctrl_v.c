/* Test helper: grabs Ctrl+V on the root window and keeps it, the way another
 * clipboard manager or a keyboard shortcut daemon would. Used by
 * linux/test-x11-conflict.sh to check that copy4paste notices the conflict
 * instead of failing silently. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

static int got_bad_access;

static int handler(Display *display, XErrorEvent *error) {
    (void)display;
    if (error->error_code == BadAccess) got_bad_access = 1;
    return 0;
}

int main(int argc, char **argv) {
    int seconds = argc > 1 ? atoi(argv[1]) : 20;
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "grab_ctrl_v: no display\n");
        return 2;
    }
    Window root = DefaultRootWindow(display);
    KeyCode v = XKeysymToKeycode(display, XK_v);
    XErrorHandler previous = XSetErrorHandler(handler);
    XGrabKey(display, v, ControlMask, root, False, GrabModeAsync, GrabModeAsync);
    XSync(display, False);
    XSetErrorHandler(previous);
    if (got_bad_access) {
        fprintf(stderr, "grab_ctrl_v: someone already owns Ctrl+V\n");
        return 1;
    }
    printf("grabbed\n");
    fflush(stdout);
    sleep((unsigned)seconds);
    XUngrabKey(display, v, AnyModifier, root);
    XSync(display, False);
    XCloseDisplay(display);
    return 0;
}
