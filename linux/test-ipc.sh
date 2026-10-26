#!/bin/sh
# Checks the fix for the reported bug: `copy4paste --select` used to start a
# second process with an empty history, so the picker never appeared on
# Wayland (where the app cannot grab the key itself).
#
# Here the clipboard is filled in the background instance a user never sees,
# then --select is run as a *separate process* and must open the picker of the
# running instance, with that instance's history.
#
#   xvfb-run -a --server-args="-screen 0 1440x900x24" sh linux/test-ipc.sh build/copy4paste
set -eu

BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
[ -x "$BIN" ] || { echo "not executable: $BIN" >&2; exit 2; }

WORK=$(mktemp -d)
export XDG_CONFIG_HOME="$WORK/config"
export XDG_RUNTIME_DIR="$WORK/run"
export HOME="$WORK/home"
mkdir -p "$HOME" "$XDG_CONFIG_HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
APP_PID=

cleanup() {
    [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null || true
    sleep 0.2
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

visible_panel() {
    xdotool search --onlyvisible --name '^copy4paste$' 2>/dev/null | head -1
}

wait_panel() {
    i=0
    while [ "$i" -lt 60 ]; do
        if [ -n "$(visible_panel)" ]; then return 0; fi
        sleep 0.05
        i=$((i + 1))
    done
    return 1
}

echo "1. no instance yet"
"$BIN" --status >/dev/null 2>&1 && fail "--status claimed an instance before starting"
# With nothing running, --select has to *become* the instance and stay: that is
# the case where there is no history to hand to anybody else.
RC=0
timeout 3 "$BIN" --select >/dev/null 2>&1 || RC=$?
[ "$RC" = "124" ] || fail "--select without an instance exited with $RC instead of running"
rm -f "$XDG_RUNTIME_DIR/copy4paste.sock"
[ -z "$(visible_panel)" ] || fail "empty picker appeared out of nowhere"

echo "2. the background instance owns the history"
"$BIN" &
APP_PID=$!
sleep 1.2
kill -0 "$APP_PID" || fail "background instance exited"
SOCKET="$XDG_RUNTIME_DIR/copy4paste.sock"
[ -S "$SOCKET" ] || fail "no socket at $SOCKET"
"$BIN" --status | grep -q 'running=1' || fail "--status does not reach the instance"

printf 'first copy' | xclip -selection clipboard -i
sleep 0.5
printf 'second copy' | xclip -selection clipboard -i
sleep 0.8
"$BIN" --status | grep -q 'entries=2' || fail "the instance did not collect the two copies"

echo "3. --select as a separate process opens that instance's picker"
"$BIN" --select || fail "--select exited with an error"
wait_panel || fail "the picker did not open from --select"
[ "$(xdotool search --onlyvisible --name '^copy4paste$' | wc -l)" = "1" ] || fail "more than one picker"

echo "4. choosing from the forwarded picker pastes the shared copy"
xdotool key --clearmodifiers 2
sleep 0.7
VALUE=$(xclip -selection clipboard -o)
[ "$VALUE" = "first copy" ] || fail "clipboard after digit 2 is '$VALUE'"

echo "5. a second plain launch does not create a second instance"
BEFORE=$(pgrep -fc "copy4paste$" || true)
"$BIN" 2>/dev/null || true
sleep 0.5
AFTER=$(pgrep -fc "copy4paste$" || true)
[ "$BEFORE" = "$AFTER" ] || fail "a second process stayed alive ($BEFORE -> $AFTER)"

echo "6. a stale socket does not lock the next launch out"
kill "$APP_PID" 2>/dev/null || true
sleep 0.6
APP_PID=
if [ -S "$SOCKET" ]; then
    echo "   (the killed instance left its socket behind, as expected on SIGKILL)"
fi
"$BIN" &
APP_PID=$!
sleep 1.2
"$BIN" --status | grep -q 'running=1' || fail "the app did not recover from a stale socket"

echo "PASS: one instance owns the history; --select forwards to it"
