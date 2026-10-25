# copy4paste 1.2.0 — Community Skins

A free, open-source, native macOS clipboard picker: copy several texts and press `⌘V` to choose which one to paste.

## What is new in 1.2.0

- **Community skins**: install a visual theme from a local `.json` file in **Settings → Import JSON skin…**. No code, scripts, or online service are required.
- Keep the four built-in skins: **Dark, Light, Cyan, Midnight**.
- Customize colors, selection highlight, corner radius, and optional font; imported skins appear alongside the built-in ones and can be selected immediately.
- Bundled community examples **Ocean** and **Sunset**, plus an editable template and simple contributor instructions in [skins/](skins/).
- Existing copies, keyboard shortcuts, opacity and privacy behavior remain unchanged. Prior numeric skin settings are supported.

## Downloads

- `copy4paste-macOS-universal.dmg` — drag-and-drop installer for **Intel macOS 10.15+** and **Apple Silicon macOS 11+**.
- `copy4paste-macOS-universal.zip` — the same app in a ZIP archive.
- `SHA256SUMS` — SHA-256 hashes for download verification.

## Installation / upgrade

1. Quit any running copy4paste.
2. Open the DMG and drag `copy4paste.app` into **Applications**, replacing the earlier copy.
3. Open `copy4paste`. If macOS blocks the first launch, follow **System Settings → Privacy & Security → Open Anyway** (or right-click → Open on supported macOS versions).
4. Enable or re-enable `copy4paste` in **Privacy & Security → Accessibility** if prompted. Replacing an ad-hoc-signed build may require granting the permission again.
5. Copy two different pieces of text, press `⌘V`, and try **Settings → Skin → Import JSON skin…**.

Copy history is still held **only in RAM**. Imported skin files are plain JSON stored under `~/Library/Application Support/copy4paste/skins/`; no network access, telemetry or accounts are added.

**Apple notarization:** this project still ships ad-hoc-signed, **not notarized** builds. See [NOTARIZATION.md](NOTARIZATION.md) for details. Avoid disabling Gatekeeper system-wide.

## Verification

- Core tests cover clipboard history and skin parsing, import, validation, legacy settings and fallback.
- CI builds a universal Intel/Apple Silicon binary and checks the minimum target versions.
- macOS interface/runtime checks, packaging, code-signature verification and mounting the DMG run in GitHub Actions.
- Ocean and Sunset were also imported and visually tested on an Intel Mac.

## Contributing skins

See [skins/README.md](skins/README.md) for the template, examples and instructions for sharing new skins through a Pull Request.
