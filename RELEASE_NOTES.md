# copy4paste 1.3.0 — Windows and Linux

A free, open-source, native clipboard picker: copy several texts and press `⌘V` (macOS) or `Ctrl+V`
(Windows and Linux) to choose which one to paste. Three platforms, one product, the same four skins.

> **Windows and Linux in this release are beta.** They are published so they can be tried, not because
> they are finished: macOS has been in use since 1.1, Windows and Linux have not been used by real
> people yet. Known limits are in the README (the Linux build is X11 only; Wayland is not supported and
> the app says so). Reports: https://github.com/KidiNoise/copy4paste/issues/6

## What is new in 1.3.0

- **Windows 10 and 11**: a single `copy4paste.exe`, no installer, no admin rights, no runtime to
  install. It waits in the notification area and intercepts `Ctrl+V` with a low-level keyboard hook.
- **Linux x86_64**: a GTK 3 build with a tarball (`./install.sh`, user only) and a `.deb`. On X11 the
  picker opens from `Ctrl+V` in any app; on Wayland it opens from a shortcut you bind to
  `copy4paste --select`, because the compositor does not allow an app to intercept `Ctrl+V`.
- **The same skins everywhere**: Dark, Light, Cyan, Midnight ship built in, and any community `.json`
  skin imports the same way. A skin written for one platform works on the other two.
- **Same behaviour**: 1–9 to choose, arrows and Enter, Backspace to delete, Esc to close, the `×` on
  each row, the gear for Settings, `Ctrl+Alt+P` as the settings shortcut, 1–30 copies, opacity, RAM
  only.
- The macOS app is unchanged apart from the version number: 1.3 is a rename of 1.2 plus the two new
  platforms.

## Downloads

- `copy4paste-macOS-universal.dmg` — drag-and-drop installer for **Intel macOS 10.15+** and
  **Apple Silicon macOS 11+**.
- `copy4paste-macOS-universal.zip` — the same app in a ZIP archive.
- `copy4paste-windows-x64.zip` — the Windows app. Unpack it and run `copy4paste.exe`.
- `copy4paste-linux-x86_64.tar.gz` — Linux build with `install.sh`, desktop entry and icon.
- `copy4paste_x86_64.deb` — the same Linux build as a Debian/Ubuntu package.
- `SHA256SUMS`, `SHA256SUMS-linux`, `SHA256SUMS-windows` — SHA-256 hashes for download verification.

## Installation / upgrade

**macOS**: quit any running copy4paste, open the DMG and drag `copy4paste.app` into **Applications**,
replacing the earlier copy. If macOS blocks the first launch, follow **System Settings → Privacy &
Security → Open Anyway** (or right-click → Open on older versions), then grant **Accessibility** again
if prompted.

**Windows**: unpack the ZIP wherever you like and run `copy4paste.exe`. SmartScreen warns once because
the build is not code-signed: **More info → Run anyway**, or unblock the file in its properties. To
start it with Windows, put a shortcut in `shell:startup`.

**Linux**: unpack the tarball and run `./install.sh` (no root: binary to `~/.local/bin`, menu entry and
icon in your home), or install the `.deb`. Requires GTK 3, libX11 and libXtst, all present on desktop
systems.

Copy history is still held **only in RAM**. Preferences and imported skins are the only things written
to disk, in the per-platform folder described in the README. No network access, telemetry or accounts.

**Code signing:** macOS still ships ad-hoc-signed and **not notarized**; Windows ships unsigned. See
[NOTARIZATION.md](NOTARIZATION.md) and the README for the exact prompts. Avoid disabling Gatekeeper or
SmartScreen system-wide.

## Verification

- Core tests (`shared/tests.c`) cover colours, skin parsing and its exact error messages, imports,
  name sanitising, the history rules and the preferences file. They run on Linux, Windows and in CI.
- **Linux**: the release build is tested against a real X server under Xvfb — `Ctrl+V` is a real key
  event through the X server, digit selection, Backspace, Esc and `Ctrl+Alt+P` are exercised, and a
  screenshot of every skin is taken.
- **Windows**: GitHub Actions builds and tests on a real Windows runner. `--verify` pushes a real
  `Ctrl+V` through the low-level hook with `SendInput`, presses `2`, and checks that the second copy
  lands on the clipboard and the panel closes. The same run also imports a community skin and opens
  Settings. The cross-compiled binary is additionally checked through Wine before release.
- `--self-test` prints the resolved skin, its colours/geometry and the history to a log on every
  platform, so a build can be checked without a human looking at it.

## Contributing skins

See [skins/README.md](skins/README.md) for the template, the examples and how to send a skin as a
Pull Request. Skins are data only and work on macOS, Windows and Linux.
