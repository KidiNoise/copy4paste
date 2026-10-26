#!/bin/sh
# End-to-end test for the Windows build.
#
# On a real Windows machine run windows/test-windows.ps1 instead. This script
# checks the cross-compiled .exe from Linux through Wine: the panel, the skins,
# an import and the whole Ctrl+V -> digit -> paste path (--verify drives it with
# SendInput, so the real low-level hook is exercised).
#
# Needs: wine64, and an X display. On a headless host:
#   xvfb-run -a --server-args="-screen 0 1440x900x24" sh windows/test-windows.sh build/copy4paste.exe
set -eu

BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
[ -f "$BIN" ] || { echo "missing $BIN" >&2; exit 2; }

WINE=${WINE:-wine64}
WINESERVER=${WINESERVER:-wineserver}
export WINEPREFIX=${WINEPREFIX:-$HOME/.wine-c4p}
export WINEDEBUG=-all
export DISPLAY=${DISPLAY:-:88}

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

# A stale Wine server from an earlier run keeps the single-instance mutex and
# the hook, so the new process would exit and Ctrl+V would go nowhere.
pkill -x wineserver 2>/dev/null || true
sleep 1
rm -rf "$WINEPREFIX"/drive_c/users/*/AppData/Roaming/copy4paste

LOG='C:\c4p-test.log'
LOG_HOST="$WINEPREFIX/drive_c/c4p-test.log"

echo "1. self-test: panel, skins and history"
rm -f "$LOG_HOST"
timeout 60 "$WINE" "$BIN" --self-test --test-seconds 3 --log "$LOG" >/dev/null 2>&1 || true
sleep 1
grep -q '^VERSION=1.3' "$LOG_HOST" || fail "version in self-test log"
grep -q '^SKINS=4' "$LOG_HOST" || fail "four built-in skins"
grep -q '^ENTRIES=4' "$LOG_HOST" || fail "seeded history"
grep -q '^PANEL_VISIBLE=1' "$LOG_HOST" || fail "panel was not visible"
grep -q '^PANEL_HEIGHT=152' "$LOG_HOST" || fail "panel is not 152 px tall"

echo "2. import a community skin"
rm -f "$LOG_HOST"
SKINS_PATH=$(cd skins/examples && pwd)/ocean.json
WIN_PATH="Z:$(echo "$SKINS_PATH" | tr '/' '\\')"
timeout 60 "$WINE" "$BIN" --import-skin "$WIN_PATH" --log "$LOG" >/dev/null 2>&1 || fail "import exited with an error"
grep -q '^IMPORTED=ocean' "$LOG_HOST" || fail "imported id"
test -f "$WINEPREFIX"/drive_c/users/*/AppData/Roaming/copy4paste/skins/ocean.json || fail "skin file not copied"

echo "3. the imported skin renders"
rm -f "$LOG_HOST"
timeout 60 "$WINE" "$BIN" --self-test --skin ocean --test-seconds 3 --log "$LOG" >/dev/null 2>&1 || true
sleep 1
grep -q '^SKIN=ocean' "$LOG_HOST" || fail "ocean skin not applied"
grep -q '^SKINS=5' "$LOG_HOST" || fail "imported skin not listed"

echo "4. Ctrl+V opens the picker, 2 pastes the second copy (SendInput through the hook)"
rm -f "$LOG_HOST"
timeout 60 "$WINE" "$BIN" --verify --log "$LOG" >/dev/null 2>&1 || true
sleep 1
grep -q '^PANEL_OPENED_BY_CTRL_V=1' "$LOG_HOST" || fail "Ctrl+V did not open the picker"
grep -q '^PANEL_VISIBLE_AFTER_DIGIT=0' "$LOG_HOST" || fail "picker stayed open after choosing"
grep -q '^RESULT=PASS' "$LOG_HOST" || fail "--verify did not pass: $(cat "$LOG_HOST" 2>/dev/null)"

echo "5. the settings window exists"
"$WINE" "$BIN" --settings >/dev/null 2>&1 &
APP_PID=$!
i=0
while [ "$i" -lt 80 ]; do
    if xdotool search --onlyvisible --name 'Settings' >/dev/null 2>&1; then break; fi
    sleep 0.1
    i=$((i + 1))
done
xdotool search --onlyvisible --name 'Settings' >/dev/null 2>&1 || fail "settings window missing"
kill "$APP_PID" 2>/dev/null || true
sleep 1

echo "PASS: windows build (panel, skins, import, Ctrl+V, digit, settings)"
