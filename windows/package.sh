#!/bin/sh
# Packages the Windows build as a zip anyone can unpack.
# Usage: windows/package.sh BUILD/copy4paste.exe 1.3.0 dist
set -eu

BIN=$1
VERSION=$2
OUT=${3:-dist}
NAME="copy4paste-$VERSION-windows-x64"
ASSET="copy4paste-windows-x64.zip"

[ -f "$BIN" ] || { echo "missing binary: $BIN" >&2; exit 2; }
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/$NAME"
install -m 644 "$BIN" "$STAGE/$NAME/copy4paste.exe"
install -m 644 windows/README.txt "$STAGE/$NAME/README.txt"
install -m 644 LICENSE "$STAGE/$NAME/LICENSE"

if command -v zip >/dev/null 2>&1; then
    (cd "$STAGE" && zip -q -r -9 "$OUT/$ASSET" "$NAME")
else
    (cd "$STAGE" && python3 -c 'import shutil,sys; shutil.make_archive(sys.argv[1], "zip", ".", sys.argv[2])' \
        "$OUT/${ASSET%.zip}" "$NAME")
fi

echo "packaged $OUT/$ASSET"
ls -l "$OUT/$ASSET"
