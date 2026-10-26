#!/bin/sh
# Installs copy4paste for the current user: no root, no system files.
# Run it from inside the unpacked folder:   ./install.sh
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
BIN_DIR="$HOME/.local/bin"
APP_DIR="$HOME/.local/share/applications"
ICON_DIR="$HOME/.local/share/icons/hicolor/512x512/apps"

mkdir -p "$BIN_DIR" "$APP_DIR" "$ICON_DIR"
install -m 755 "$HERE/copy4paste" "$BIN_DIR/copy4paste"
install -m 644 "$HERE/copy4paste.desktop" "$APP_DIR/copy4paste.desktop"
install -m 644 "$HERE/copy4paste.png" "$ICON_DIR/copy4paste.png"

# Refresh the menu entry when the tools are available (not required).
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APP_DIR" >/dev/null 2>&1 || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" >/dev/null 2>&1 || true

cat <<'MESSAGE'
copy4paste installed.

  Start it now:        ~/.local/bin/copy4paste &
  Or find "copy4paste" in your applications menu.
  Settings:            Ctrl+Alt+P      Quit: Settings > Quit

On X11 sessions, Ctrl+V opens the picker from any app.
This is a BETA build. A Wayland session is not supported: Wayland gives an
application no way to read the clipboard in the background or to intercept
Ctrl+V, so copy4paste refuses to start there and explains why. Log in with an
X11 session (or Xorg) to use it.
MESSAGE

case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *) echo ""; echo "Note: $BIN_DIR is not in your PATH. Add it to use 'copy4paste' directly." ;;
esac
