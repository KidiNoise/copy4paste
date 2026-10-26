#!/bin/sh
# Builds the Linux app. Needs: libgtk-3-dev, libx11-dev, libxtst-dev, pkg-config, cc.
set -eu

OUT=${1:-build}
mkdir -p "$OUT"

CC=${CC:-cc}
CFLAGS="-std=c11 -O2 -Wall -Wextra -Werror -Ishared"

# shellcheck disable=SC2086
$CC $CFLAGS $(pkg-config --cflags gtk+-3.0) \
    linux/main.c \
    shared/json.c shared/skins.c shared/skins_list.c shared/history.c shared/config.c \
    $(pkg-config --libs gtk+-3.0) -lX11 -lXtst -lm \
    -o "$OUT/copy4paste"

echo "built $OUT/copy4paste"
