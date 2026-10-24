# copy4paste 1.1

The clipboard with history for Mac. Copy several texts and choose which one to paste with one key.

## Download

- `copy4paste-macOS-universal.dmg` — universal installer (**Intel 10.15 Catalina+** and
  **Apple Silicon 11 Big Sur+**), HFS+ DMG.
- `copy4paste-macOS-universal.zip` — the same app, zipped.
- `SHA256SUMS` — checksums to verify the download.

## Installation

1. Open the DMG and drag `copy4paste.app` to **Applications**.
2. First launch: **right-click → Open**.
3. System Settings → Privacy & Security → **Accessibility** → enable `copy4paste`.
4. Copy two different texts and press `⌘V`.

## What is new in 1.1

- **⚙** button in the picker to open Settings.
- **Copies**: 4 by default, adjustable from 1 to 30.
- **Skins**: Dark, Light, Cyan and Midnight.
- **Opacity**: editable percentage (20–100 %), no animations, no timers.
- Selection and deletion act on the visible snapshot, so a clipboard change cannot make the app
  delete or paste the wrong entry.
- **Quit** button in Settings.

## Important notice

The binary is **signed ad hoc** and **not notarized by Apple**: there is no Apple Developer account
behind the project. Gatekeeper will ask for confirmation the first time. On Catalina or later:
right-click `copy4paste.app` → **Open**. On macOS 15 and later you may need System Settings →
Privacy & Security → **Open Anyway** instead. The Accessibility permission is tied to the signature,
so every rebuild needs it granted again.

## Verification

- 18 history checks in `macos/Tests/main.swift`.
- Interface test (`--verify-ui`): one cross per row, cross receives clicks, text does not block
  clicks, selection sticks and deletion is wired.
- Runtime test (`--verify-runtime`): history read from the real clipboard (two texts, in order) and
  event tap state.
- Universal binary verified with `lipo` and minimum versions with `vtool`.
- DMG mounted in CI and the binary inside the image checked.
- The real `⌘V` shortcut and the floating picker were tested on a Mac running
  **macOS Catalina 10.15.7 (Intel)**.

Built on GitHub Actions on `macos-15-intel` from this repository. No external dependencies: system
frameworks only.
