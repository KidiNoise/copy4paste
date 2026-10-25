# Community skins

A **skin** is a small `.json` file that changes how copy4paste looks: colours,
the highlight of the selected row, the corners of the panel and (optionally) the
font. A skin is **only** data. It never runs code, never touches the clipboard,
and never changes what you copy, paste or store.

> copy4paste ships with four built-in skins — **Dark**, **Light**, **Cyan** and
> **Midnight** — which cannot be removed. Everything you import lives alongside
> them in `~/Library/Application Support/copy4paste/skins/`.

## Make your first skin (no programming)

1. Copy [`template.json`](template.json) and rename the copy, for example
   `sunset.json`.
2. Open it in any text editor (TextEdit works) and change the values.
3. Save it and import it: open copy4paste → `⚙` → **Import JSON skin…** → pick
   your file. It applies immediately.

The file name (without `.json`) becomes the skin's id. Two skins with the same
file name are the same skin: importing one replaces the other.

## The format

```json
{
  "name": "Sunset",
  "author": "your name",
  "colors": {
    "background": "#2B1B24",
    "selection": "#C2410CD9",
    "foreground": "#FDEBD0",
    "muted": "#D6A98F",
    "delete": "#F4C6A6"
  },
  "appearance": "dark",
  "cornerRadius": 12,
  "font": {
    "name": "Menlo",
    "size": 12
  }
}
```

### `name` (required)

The name shown in the Settings selector.

### `author` (optional)

Your name or handle.

### `colors` (required)

All five colours are required. Each is a hex string in one of two forms:

| Form | Example | Meaning |
|---|---|---|
| `#RRGGBB` | `#2B1B24` | Red, green, blue, fully opaque |
| `#RRGGBBAA` | `#C2410CD9` | Red, green, blue + alpha (transparency) |

| Key | What it colours |
|---|---|
| `background` | The panel behind the picker and Settings |
| `selection` | The highlight behind the row you are pointing at |
| `foreground` | The copied text and main labels |
| `muted` | The small hints, headers and icons |
| `delete` | The × of each row |

Use 8-digit hex (`#RRGGBBAA`) for the selection colour if you want a
translucent highlight. The panel background is also multiplied by the
**Opacity** setting in Settings.

### `appearance` (optional)

`"dark"` or `"light"`. This only picks the right look for the small controls
(stepper, pop-up, text field). Default is `"dark"`.

### `cornerRadius` (optional)

How rounded the panel corners are, from `0` (sharp) to `30` (very round).
Default is `9`.

### `font` (optional)

| Key | What it does |
|---|---|
| `name` | A font name available on macOS, e.g. `"Menlo"` or `"Avenir Next"`. If it is not installed, copy4paste silently uses the system font. |
| `size` | The size of the copied text, from `8` to `36`. Default is `12`. |

Both keys are optional and can be left out.

## Rules

- The file must be valid JSON and use the fields above.
- Colours must be `#RRGGBB` or `#RRGGBBAA` hex strings.
- Nothing in a skin file is ever executed — it is read as data only.
- If a skin is broken, copy4paste falls back to **Dark** instead of crashing.

## Share your skin (Pull Request)

1. Fork this repository.
2. Add your `.json` file to `skins/examples/` (one skin per file).
3. Commit and open a **Pull Request**.
4. Name the PR after your skin, for example `Add "Sunset" skin`, and include a
   short note about the palette.

Keep the file name short, lowercase and free of special characters (spaces and
`-` are fine). Avoid `dark`, `light`, `cyan` and `midnight` — they are reserved
by the built-in skins and the app renames any import that collides.

If you want a colour picker, macOS has one built in: open *Digital Color
Meter* or use the system colour panel in any editor to grab hex values.
