#!/bin/sh
# Takes the screenshots used as evidence: the picker in every skin, and the
# settings window. Needs a running X server (Xvfb is fine), xdotool and
# ImageMagick's `import`.
#
#   xvfb-run -a --server-args="-screen 0 1440x900x24" linux/capturas.sh build/copy4paste capturas
set -eu

BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
OUT=${2:-capturas}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

# A throwaway configuration, so the real one is never touched.
XDG_CONFIG_HOME=$(mktemp -d)
export XDG_CONFIG_HOME
trap 'rm -rf "$XDG_CONFIG_HOME"' EXIT

"$BIN" --import-skin skins/examples/ocean.json >/dev/null
"$BIN" --import-skin skins/examples/sunset.json >/dev/null

xdotool mousemove 600 400 || true

for skin in dark light cyan midnight ocean sunset; do
    "$BIN" --self-test --skin "$skin" --test-seconds 10 >/dev/null 2>&1 &
    PID=$!
    sleep 2
    WINDOW=$(xdotool search --onlyvisible --name '^copy4paste$' | head -1 || true)
    if [ -z "$WINDOW" ]; then
        echo "no panel for skin $skin" >&2
        kill "$PID" 2>/dev/null || true
        exit 1
    fi
    import -window "$WINDOW" "$OUT/panel-$skin.png"
    kill "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true
    sleep 0.3
done

"$BIN" --settings >/dev/null 2>&1 &
PID=$!
sleep 2
WINDOW=$(xdotool search --onlyvisible --name 'Settings' | head -1 || true)
if [ -n "$WINDOW" ]; then
    import -window "$WINDOW" "$OUT/settings.png"
fi
kill "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true

echo "screenshots in $OUT"
ls -1 "$OUT"
