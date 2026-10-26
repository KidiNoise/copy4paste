#!/bin/sh
# Cross-compiles the Windows build from Linux (or any host with mingw-w64).
# Needs: x86_64-w64-mingw32-gcc and x86_64-w64-mingw32-windres.
set -eu

OUT=${1:-build}
# Linux cross-builds with the x86_64-w64-mingw32-* tools; inside MSYS2/MINGW64
# the same compiler is simply called gcc/windres.
if [ -z "${CROSS:-}" ]; then
    if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        CROSS=x86_64-w64-mingw32-gcc
    else
        CROSS=gcc
    fi
fi
if [ -z "${WINDRES:-}" ]; then
    if command -v x86_64-w64-mingw32-windres >/dev/null 2>&1; then
        WINDRES=x86_64-w64-mingw32-windres
    else
        WINDRES=windres
    fi
fi
mkdir -p "$OUT"

"$WINDRES" -I windows windows/app.rc -O coff -o "$OUT/app.res.o"

$CROSS -std=c11 -O2 -Wall -Wextra -Werror -Ishared -municode \
    windows/main.c \
    shared/json.c shared/skins.c shared/skins_list.c shared/history.c shared/config.c \
    "$OUT/app.res.o" \
    -o "$OUT/copy4paste.exe" \
    -mwindows -lcomctl32 -lshell32 -lcomdlg32 -lgdi32 -luser32 -lole32

echo "built $OUT/copy4paste.exe"
