# Web del proyecto

Landing pública de copy4paste, servida con **GitHub Pages** desde esta carpeta (`main` → `/docs`).

- `index.html` — la página. HTML plano, sin framework.
- `src/input.css` — fuente de los estilos (Tailwind + `@font-face` + ajustes base).
- `tailwind.config.js` — tokens de color, tipografía y espaciado del diseño.
- `assets/style.css` — CSS compilado y minificado. **Es lo que se sirve.**
- `assets/fonts/` — Inter, JetBrains Mono y un subconjunto de Material Symbols (15 iconos), servidos
  desde el propio sitio: la página no llama a Google Fonts.
- `assets/og.png` — imagen para vistas previas en redes (1200×630).

## Cambiar textos o secciones

Se edita `index.html` y se recompila el CSS si aparecen clases nuevas:

```bash
npx tailwindcss@3.4.17 -c docs/tailwind.config.js -i docs/src/input.css -o docs/assets/style.css --minify
```

## Iconos

Los iconos son caracteres del subconjunto de Material Symbols, no SVG ni una fuente remota. Si hace
falta un icono nuevo, hay que añadir su código al subconjunto:

```bash
# 1. Localizar el código del icono en la fuente completa
python3 - <<'PY'
from fontTools.ttLib import TTFont
f = TTFont('MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].woff2')
inv = {v: k for k, v in f.getBestCmap().items()}
print(hex(inv['nombre_del_icono']))
PY

# 2. Regenerar el subconjunto con todos los códigos usados
pyftsubset fuente-completa.woff2 --output-file=docs/assets/fonts/material-symbols.woff2 \
  --flavor=woff2 --unicodes="U+E14F,U+E5D2,..." --no-hinting --desubroutinize
```

En el HTML se escribe el carácter con su entidad (`&#xE14F;`) y `aria-hidden="true"`.

## Comprobado

Capturas reales con Chromium a **390 px** y **1440 px**: sin desbordamiento horizontal, sin errores de
consola, tipografía 36/56 px en el titular y 28/40 px en los subtítulos.
