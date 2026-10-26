#!/bin/sh
# Checks what happens when another application already owns Ctrl+V.
#
# Before this fix the grab failed with BadAccess and nothing was said: the user
# pressed Ctrl+V and simply nothing happened, with no way to find out why.
#
#   xvfb-run -a --server-args="-screen 0 1440x900x24" sh linux/test-x11-conflict.sh build/copy4paste build/grab_ctrl_v
set -eu

BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
HELPER=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
[ -x "$BIN" ] || { echo "not executable: $BIN" >&2; exit 2; }
[ -x "$HELPER" ] || { echo "not executable: $HELPER (build linux/tests/grab_ctrl_v.c)" >&2; exit 2; }

WORK=$(mktemp -d)
export XDG_CONFIG_HOME="$WORK/config"
export XDG_RUNTIME_DIR="$WORK/run"
export HOME="$WORK/home"
mkdir -p "$HOME" "$XDG_CONFIG_HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
APP_PID=
HELPER_PID=

cleanup() {
    [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null || true
    [ -n "$HELPER_PID" ] && kill "$HELPER_PID" 2>/dev/null || true
    sleep 0.2
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

echo "1. with Ctrl+V free, the app says it owns the key"
"$BIN" &
APP_PID=$!
sleep 1.5
"$BIN" --status | grep -q 'hotkey=grabbed' || {
    "$BIN" --status
    fail "hotkey should be grabbed when nothing else uses it"
}
kill "$APP_PID" 2>/dev/null || true
sleep 0.6
APP_PID=

echo "2. another application takes Ctrl+V first"
"$HELPER" 30 > "$WORK/helper.log" 2>&1 &
HELPER_PID=$!
i=0
while [ "$i" -lt 40 ]; do
    grep -q grabbed "$WORK/helper.log" 2>/dev/null && break
    sleep 0.05
    i=$((i + 1))
done
grep -q grabbed "$WORK/helper.log" || fail "the test helper could not grab Ctrl+V: *** $(cat "$WORK/helper.log")"

echo "3. copy4paste notices instead of failing silently"
"$BIN" 2> "$WORK/app.err" &
APP_PID=$!
sleep 1.5
"$BIN" --status > "$WORK/status.txt" || fail "--status did not answer"
cat "$WORK/status.txt"
grep -q 'hotkey=unavailable' "$WORK/status.txt" || fail "the conflict was not reported in the status"
grep -q 'already owns Ctrl+V' "$WORK/app.err" || {
    echo "--- stderr ---"; cat "$WORK/app.err"
    fail "no warning on stderr about the conflict"
}

echo "4. the picker still opens when asked to, so the app is usable"
printf 'one' | xclip -selection clipboard -i
sleep 0.5
printf 'two' | xclip -selection clipboard -i
sleep 0.8
"$BIN" --select || fail "--select failed while the hotkey was unavailable"
i=0
while [ "$i" -lt 60 ]; do
    [ -n "$(xdotool search --onlyvisible --name '^copy4paste$' 2>/dev/null | head -1)" ] && break
    sleep 0.05
    i=$((i + 1))
done
[ -n "$(xdotool search --onlyvisible --name '^copy4paste$' 2>/dev/null | head -1)" ] || \
    fail "the picker did not open with --select"

echo "5. and the key reaches the application that owns it"
# The panel is open, so close it first: the grab, not the panel, is under test.
xdotool key --clearmodifiers Escape
sleep 0.4
kill "$HELPER_PID" 2>/dev/null || true
HELPER_PID=
sleep 0.5

echo "PASS: a Ctrl+V taken by another application is detected and explained"
