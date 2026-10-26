copy4paste for Linux
====================

Choose which of your last copies to paste. Press Ctrl+V and a small picker
appears where the cursor is: press 1-9 to paste that copy, use the arrow keys
and Enter, Backspace to delete a copy, Esc to close. Ctrl+Alt+P opens Settings.

The history lives in RAM and disappears when the app quits. Preferences and
imported skins go to ~/.config/copy4paste/. No network, no telemetry, no
account.

Install
-------

  ./install.sh

That copies the binary to ~/.local/bin, adds the menu entry and the icon for
your user only (no root, nothing outside your home). Then run

  copy4paste &

or pick "copy4paste" from the applications menu. To start it automatically when
you log in, add it to your desktop's startup applications.

Requirements: GTK 3 (already present on GNOME, KDE, XFCE, Cinnamon…), plus
libX11 and libXtst for the global shortcut. Without XTEST the picker still
opens from `copy4paste --select` and the app cannot send Ctrl+V for you.

Wayland
-------

On a Wayland session an application is not allowed to intercept Ctrl+V, so the
picker has to be started from a shortcut you bind yourself:

  copy4paste --select

GNOME: Settings > Keyboard > Custom Shortcuts > add a shortcut that runs
`copy4paste --select`. KDE: System Settings > Shortcuts. On X11 (or a session
running through XWayland) Ctrl+V works by itself.

Skins
-----

Four skins ship with the app (Dark, Light, Cyan, Midnight). Download any
community skin from the project site, then Settings > Import JSON skin…
Imported skins are plain JSON files in ~/.config/copy4paste/skins/.

Uninstall
---------

  rm ~/.local/bin/copy4paste
  rm ~/.local/share/applications/copy4paste.desktop
  rm ~/.local/share/icons/hicolor/512x512/apps/copy4paste.png
  rm -r ~/.config/copy4paste          # preferences and imported skins

License: MIT. Source: https://github.com/KidiNoise/copy4paste
