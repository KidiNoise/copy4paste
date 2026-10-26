#!/bin/sh
# Packages the Linux build: a tarball anyone can unpack and a .deb.
# Usage: linux/package.sh BUILD/copy4paste 1.3.0 dist
set -eu

BIN=$1
VERSION=$2
OUT=${3:-dist}
ARCH=x86_64
NAME="copy4paste-$VERSION-linux-$ARCH"
# Asset names stay stable across releases: the website links to them.
ASSET_TAR="copy4paste-linux-$ARCH.tar.gz"
ASSET_DEB="copy4paste_${ARCH}.deb"

[ -x "$BIN" ] || { echo "missing binary: $BIN" >&2; exit 2; }
mkdir -p "$OUT"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

# --- tarball -----------------------------------------------------------
mkdir -p "$STAGE/$NAME"
install -m 755 "$BIN" "$STAGE/$NAME/copy4paste"
install -m 644 linux/copy4paste.desktop "$STAGE/$NAME/copy4paste.desktop"
install -m 644 linux/icon.png "$STAGE/$NAME/copy4paste.png"
install -m 755 linux/install.sh "$STAGE/$NAME/install.sh"
install -m 644 linux/README.txt "$STAGE/$NAME/README.txt"
install -m 644 LICENSE "$STAGE/$NAME/LICENSE"
tar -C "$STAGE" -czf "$OUT/$ASSET_TAR" "$NAME"

# --- deb ---------------------------------------------------------------
if command -v dpkg-deb >/dev/null 2>&1; then
    DEB="$STAGE/deb"
    mkdir -p "$DEB/DEBIAN" "$DEB/usr/bin" "$DEB/usr/share/applications" \
             "$DEB/usr/share/icons/hicolor/512x512/apps" "$DEB/usr/share/doc/copy4paste"
    install -m 755 "$BIN" "$DEB/usr/bin/copy4paste"
    install -m 644 linux/copy4paste.desktop "$DEB/usr/share/applications/copy4paste.desktop"
    install -m 644 linux/icon.png "$DEB/usr/share/icons/hicolor/512x512/apps/copy4paste.png"
    install -m 644 LICENSE "$DEB/usr/share/doc/copy4paste/copyright"
    install -m 644 linux/README.txt "$DEB/usr/share/doc/copy4paste/README"
    gzip -9n "$DEB/usr/share/doc/copy4paste/README"
    SIZE=$(du -sk --exclude=DEBIAN "$DEB" | cut -f1)
    cat > "$DEB/DEBIAN/control" <<CONTROL
Package: copy4paste
Version: $VERSION
Section: utils
Priority: optional
Architecture: amd64
Installed-Size: $SIZE
Maintainer: KidiNoise <https://github.com/KidiNoise>
Depends: libgtk-3-0, libx11-6, libxtst6
Homepage: https://kidinoise.github.io/copy4paste/
Description: choose which of your last copies to paste
 copy4paste keeps your last copies in RAM. Press Ctrl+V and a small picker
 appears: pick a copy with 1-9 or the arrow keys, Backspace deletes one, Esc
 closes. Skins are plain JSON files. No network, no telemetry, no history on
 disk.
CONTROL
    dpkg-deb --root-owner-group -Zxz --build "$DEB" "$OUT/$ASSET_DEB" >/dev/null
else
    echo "dpkg-deb not found: skipping the .deb (the tarball was built)" >&2
fi

echo "packaged $OUT/$ASSET_TAR"
ls -l "$OUT" | sed -n '1,10p'
