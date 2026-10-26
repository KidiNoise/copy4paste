#!/bin/sh
# End-to-end test for the Linux build. Runs against a real X server;
# on a headless machine use:  xvfb-run -a linux/test-linux.sh build/copy4paste
#
# Needs: xclip, xdotool. It never touches the real configuration: the app runs
# with a throwaway XDG_CONFIG_HOME.
set -eu

BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
[ -x "$BIN" ] || { echo "not executable: $BIN" >&2; exit 2; }

WORK=$(mktemp -d)
export XDG_CONFIG_HOME="$WORK/config"
export HOME="$WORK/home"
mkdir -p "$HOME" "$XDG_CONFIG_HOME"
APP_PID=

cleanup() {
    [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null || true
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
    while [ "$i" -lt 40 ]; do
        if [ -n "$(visible_panel)" ]; then return 0; fi
        sleep 0.05
        i=$((i + 1))
    done
    return 1
}

wait_no_panel() {
    i=0
    while [ "$i" -lt 40 ]; do
        if [ -z "$(visible_panel)" ]; then return 0; fi
        sleep 0.05
        i=$((i + 1))
    done
    return 1
}

echo "1. version"
"$BIN" --version | grep -q '^copy4paste 1\.3' || fail "version string"

echo "2. four built-in skins without any configuration"
"$BIN" --version-json | grep -q '"skins":4' || fail "built-in skin count"

echo "3. import a community skin"
ID=$("$BIN" --import-skin skins/examples/ocean.json)
[ "$ID" = "ocean" ] || fail "imported id was '$ID'"
"$BIN" --version-json | grep -q '"skins":5' || fail "imported skin not listed"
test -f "$XDG_CONFIG_HOME/copy4paste/skins/ocean.json" || fail "skin file not copied"
"$BIN" --import-skin skins/template.json >/dev/null || fail "template import"
"$BIN" --import-skin "$WORK/does-not-exist.json" 2>/dev/null && fail "missing file accepted"
grep -q '^skin=template' "$XDG_CONFIG_HOME/copy4paste/settings.ini" || fail "import not stored as current skin"

echo "4. start the app"
"$BIN" &
APP_PID=$!
sleep 1
kill -0 "$APP_PID" || fail "app exited on startup"
[ -z "$(visible_panel)" ] || fail "panel visible before any copy"

echo "5. one copy only: Ctrl+V is left alone"
printf 'onlyone' | xclip -selection clipboard -i
sleep 0.9
xdotool key --clearmodifiers ctrl+v
sleep 0.5
[ -z "$(visible_panel)" ] || fail "panel opened with a single copy"
VALUE=$(xclip -selection clipboard -o)
[ "$VALUE" = "onlyone" ] || fail "single copy changed the clipboard ('$VALUE')"

echo "5b. two different copies"
printf 'alpha' | xclip -selection clipboard -i
sleep 0.5
printf 'beta' | xclip -selection clipboard -i
sleep 0.9

echo "6. Ctrl+V opens the picker"
xdotool key --clearmodifiers ctrl+v
wait_panel || fail "panel did not open on Ctrl+V"

echo "7. digit 2 pastes the second copy and closes the picker"
xdotool key --clearmodifiers 2
sleep 0.6
[ -z "$(visible_panel)" ] || fail "panel still visible after choosing"
VALUE=$(xclip -selection clipboard -o)
[ "$VALUE" = "alpha" ] || fail "clipboard after choosing 2 is '$VALUE'"

echo "9. Backspace deletes the selected copy"
printf 'two' | xclip -selection clipboard -i
sleep 0.6
xdotool key --clearmodifiers ctrl+v
wait_panel || fail "panel did not open again"
xdotool key --clearmodifiers BackSpace
sleep 0.5
VALUE=$(xclip -selection clipboard -o)
[ "$VALUE" != "two" ] || fail "deleted copy is still on the clipboard"
[ "$VALUE" = "beta" ] || fail "clipboard after delete is '$VALUE', expected the next copy"

echo "10. Escape closes the picker"
xdotool key --clearmodifiers Escape
sleep 0.4
[ -z "$(visible_panel)" ] || fail "Escape left the panel open"

echo "11. Ctrl+Alt+P opens Settings"
xdotool key --clearmodifiers ctrl+alt+p
i=0
while [ "$i" -lt 40 ]; do
    if xdotool search --onlyvisible --name 'Settings' >/dev/null 2>&1; then break; fi
    sleep 0.05
    i=$((i + 1))
done
xdotool search --onlyvisible --name 'Settings' >/dev/null 2>&1 || fail "settings window missing"

echo "12. the app is still running and sound"
kill -0 "$APP_PID" || fail "app died"
kill "$APP_PID" 2>/dev/null || true
APP_PID=
echo "PASS: linux build (version, skins, import, Ctrl+V, digits, delete, Esc, settings)"
