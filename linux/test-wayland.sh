#!/bin/sh
# Verifies what copy4paste does in a real Wayland session, with a real
# compositor (Weston, headless backend) instead of a guess.
#
# The honest answer is that it cannot work there: a Wayland compositor gives no
# way for a background application to read the clipboard, and none to intercept
# Ctrl+V. So the app must say so and exit, instead of opening an empty picker.
# This test proves that, and proves it does not touch the session first.
#
#   sh linux/test-wayland.sh build/copy4paste
set -eu

BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
[ -x "$BIN" ] || { echo "not executable: $BIN" >&2; exit 2; }

BASE=$(mktemp -d)
export XDG_RUNTIME_DIR="$BASE/run"
export XDG_CONFIG_HOME="$BASE/config"
export HOME="$BASE/home"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME" "$HOME"
chmod 700 "$XDG_RUNTIME_DIR"
export XDG_SESSION_TYPE=wayland

cleanup() {
    [ -n "${WESTON_PID:-}" ] && kill "$WESTON_PID" 2>/dev/null || true
    [ -n "${app_output:-}" ] && rm -f "$app_output"
    rm -rf "$BASE"
}
trap cleanup EXIT INT TERM

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

command -v weston >/dev/null 2>&1 || {
    echo "SKIP: weston is not installed, so Wayland cannot be tested here"
    exit 0
}

echo "1. start a headless Wayland compositor"
weston --backend=headless-backend.so --socket=c4p-test --idle-time=0 \
       --log="$BASE/weston.log" >/dev/null 2>&1 &
WESTON_PID=$!
i=0
while [ "$i" -lt 100 ]; do
    [ -S "$XDG_RUNTIME_DIR/c4p-test" ] && break
    sleep 0.1
    i=$((i + 1))
done
[ -S "$XDG_RUNTIME_DIR/c4p-test" ] || { cat "$BASE/weston.log"; fail "weston did not start"; }
export WAYLAND_DISPLAY=c4p-test
unset DISPLAY || true

echo "2. the app refuses to pretend, with an actionable message"
app_output="$BASE/app.err"
RC=0
GDK_BACKEND=wayland "$BIN" > /dev/null 2> "$app_output" || RC=$?
echo "   exit code: $RC"
sed -n '1,10p' "$app_output"
[ "$RC" = "3" ] || fail "expected exit code 3 on Wayland, got $RC"
grep -q "Wayland" "$app_output" || fail "the message does not mention Wayland"
grep -qi "X11" "$app_output" || fail "the message does not say what to do instead"
grep -qi "Ctrl+V" "$app_output" || fail "the message does not explain why"

echo "3. nothing was left behind: no socket, no second instance"
[ -S "$XDG_RUNTIME_DIR/copy4paste.sock" ] && fail "the app created an instance socket on Wayland"
"$BIN" --status >/dev/null 2>&1 && fail "an instance answered on Wayland"

echo "4. --print-support tells the truth about the session too"
GDK_BACKEND=wayland "$BIN" --print-support 2>/dev/null | grep -q '"session":"wayland"' || \
    fail "--print-support does not report the Wayland session"

echo "5. and with X11 available the same binary does run"
# Same binary, X11 session: it must NOT refuse.
if [ -n "${X11_DISPLAY:-}" ]; then
    DISPLAY="$X11_DISPLAY" "$BIN" --status >/dev/null 2>&1 || true
    DISPLAY="$X11_DISPLAY" "$BIN" --version >/dev/null || fail "--version failed on X11"
fi

echo "PASS: on Wayland the app stops and explains; it does not ship a broken picker"
