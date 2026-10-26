copy4paste for Linux (BETA)
===========================

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

Wayland: not supported
-----------------------

A Wayland compositor gives an application no way to read the clipboard in the
background, and none to intercept Ctrl+V. There is therefore no history to
show, and copy4paste stops at startup with a message saying so, instead of
opening an empty picker.

Use an X11 session (Xorg). If your desktop offers "X11" at the login screen,
choose it. `echo $XDG_SESSION_TYPE` says which one you are in.

This is a known limitation, tracked in the project issue tracker, not something
a shortcut can work around.

One instance, several commands
------------------------------

The history lives in the running instance. `copy4paste --select` and
`copy4paste --settings` talk to it over a socket in $XDG_RUNTIME_DIR, so a
shortcut or a menu entry reaches the same history:

  copy4paste --select      open the picker (bind it to a key you like)
  copy4paste --settings    open Settings
  copy4paste --status      what the running instance is doing

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
