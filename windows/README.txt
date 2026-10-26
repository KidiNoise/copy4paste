copy4paste for Windows
======================

Choose which of your last copies to paste. Press Ctrl+V and a small picker
appears next to the cursor: press 1-9 to paste that copy, use the arrow keys
and Enter, Backspace to delete a copy, Esc to close. Ctrl+Alt+P opens Settings.

The history lives in RAM and disappears when the app quits. Preferences and
imported skins go to %APPDATA%\copy4paste. No network, no telemetry, no account.

Run it
------

Double-click copy4paste.exe. It starts in the notification area (next to the
clock): right-click the icon for "Open picker", "Settings" and "Quit". There is
no window at launch, and no installer: the .exe is the whole app.

Windows 10/11 64-bit. Nothing else to install.

The first time, SmartScreen may say "Windows protected your PC" because the
build is not code-signed. Click "More info" then "Run anyway". If you prefer,
right-click the .exe > Properties and tick "Unblock" before running it.

Start with Windows
------------------

Press Win+R, type  shell:startup  and press Enter. Put a shortcut to
copy4paste.exe in that folder.

Skins
-----

Four skins ship with the app (Dark, Light, Cyan, Midnight). Download any
community skin from the project site, then Settings > Import JSON skin…
Imported skins are ordinary JSON files in %APPDATA%\copy4paste\skins.

Uninstall
---------

Quit from the tray menu, delete copy4paste.exe, and remove the folder
%APPDATA%\copy4paste (preferences and imported skins).

License: MIT. Source: https://github.com/KidiNoise/copy4paste
