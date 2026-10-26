#!/bin/sh
# Cross-compiles the Windows build from Linux (or any host with mingw-w64).
# Needs: x86_64-w64-mingw32-gcc and x86_64-w64-mingw32-windres.
set -eu

OUT=${1:-build}
CROSS=${CROSS:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
mkdir -p "$OUT"

"$WINDRES" -I windows windows/app.rc -O coff -o "$OUT/app.res.o"

$CROSS -std=c11 -O2 -Wall -Wextra -Werror -Ishared -municode \
    windows/main.c \
    shared/json.c shared/skins.c shared/skins_list.c shared/history.c shared/config.c \
    "$OUT/app.res.o" \
    -o "$OUT/copy4paste.exe" \
    -mwindows -lcomctl32 -lshell32 -lcomdlg32 -lgdi32 -luser32 -lole32

echo "built $OUT/copy4paste.exe"
