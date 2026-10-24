# Project website

Public landing page for copy4paste, served with **GitHub Pages** from this folder (`main` → `/docs`).

- `index.html` — the page. Plain HTML, no framework.
- `src/input.css` — source styles (Tailwind + `@font-face` + base tweaks).
- `tailwind.config.js` — colour, type and spacing tokens for the design.
- `assets/style.css` — compiled, minified CSS. **This is what gets served.**
- `assets/fonts/` — Inter, JetBrains Mono and a subset of Material Symbols (15 icons), served from
  this site: the page makes no calls to Google Fonts.
- `assets/og.png` — social preview image (1200×630).
- `assets/readme-hero.png` — the screenshot used at the top of the repository README.

## Changing text or sections

Edit `index.html` and rebuild the CSS if new classes appear:

```bash
npx tailwindcss@3.4.17 -c docs/tailwind.config.js -i docs/src/input.css -o docs/assets/style.css --minify
```

## Icons

Icons are characters from the Material Symbols subset, not SVG and not a remote font. Adding a new one
means adding its codepoint to the subset:

```bash
# 1. Look up the icon codepoint in the full font
python3 - <<'PY'
from fontTools.ttLib import TTFont
f = TTFont('MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].woff2')
inv = {v: k for k, v in f.getBestCmap().items()}
print(hex(inv['icon_name']))
PY

# 2. Regenerate the subset with every codepoint in use
pyftsubset full-font.woff2 --output-file=docs/assets/fonts/material-symbols.woff2 \
  --flavor=woff2 --unicodes="U+E14F,U+E5D2,..." --no-hinting --desubroutinize
```

In the HTML the character is written as its entity (`&#xE14F;`) with `aria-hidden="true"`.

## Screenshots

`readme-hero.png` and `og.png` are real captures of this page taken with Chromium. Regenerate them
after text changes so the README and the social card never drift from the site.

## Verified

Real Chromium captures at **390 px** and **1440 px**: no horizontal overflow, no console errors,
36/56 px headline and 28/40 px subheads.
