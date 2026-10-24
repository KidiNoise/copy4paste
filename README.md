<p align="center">
  <a href="https://kidinoise.github.io/copy4paste/">
    <img src="docs/assets/readme-hero.png" alt="copy4paste — Copia. Elige. Pega." width="100%">
  </a>
</p>

<p align="center">
  <a href="https://github.com/KidiNoise/copy4paste/releases/latest"><img alt="Versión" src="https://img.shields.io/badge/versi%C3%B3n-1.1-0071e3"></a>
  <img alt="macOS" src="https://img.shields.io/badge/macOS-10.15%2B%20Intel%20%C2%B7%2011%2B%20Apple%20Silicon-1b1b1d">
  <img alt="Licencia" src="https://img.shields.io/badge/licencia-MIT-006a26">
  <img alt="Sin dependencias" src="https://img.shields.io/badge/dependencias-ninguna-717785">
</p>

<p align="center">
  <b><a href="https://github.com/KidiNoise/copy4paste/releases/latest/download/copy4paste-macOS-universal.dmg">Descargar para Mac</a></b>
  &nbsp;·&nbsp;
  <a href="https://kidinoise.github.io/copy4paste/">Web del proyecto</a>
  &nbsp;·&nbsp;
  <a href="#instalación">Cómo se instala</a>
</p>

---

Copia varios textos y elige cuál pegar con una tecla. Al pulsar `⌘V` aparece un selector flotante con
tus últimas copias; pulsas `1`, `2`, `3`… y sigue tu trabajo. Nativo de macOS, sin Electron, sin red y
sin historial en disco.

## Instalación

1. Abre el **DMG** y arrastra `copy4paste.app` a **Aplicaciones**.
2. Primer arranque: **clic derecho → Abrir**.
3. Ajustes del Sistema → Privacidad y seguridad → **Accesibilidad** → activa `copy4paste`.
4. Copia dos textos distintos y pulsa `⌘V`.

> **Si macOS dice que no puede abrir la app** (macOS 15 o posterior ya no acepta «clic derecho → Abrir»
> en todos los casos): Ajustes del Sistema → Privacidad y seguridad → baja hasta el aviso sobre
> `copy4paste` → **Abrir igualmente**. Solo hace falta la primera vez. Pasa porque el binario va firmado
> *ad hoc* y no notarizado: no hay cuenta Apple Developer detrás del proyecto. El código es abierto y
> se puede compilar con las instrucciones de abajo.

## Qué hace

- `⌘C` guarda los últimos **4 textos** (ajustable de 1 a 30).
- `⌘V`: selector con la copia **más reciente arriba**.
  - `1`–`9` pega esa copia · `↑`/`↓` + `Intro` navega · `Retroceso` borra sin pegar · `Esc` cancela.
  - `×` a la derecha de cada fila borra esa entrada con el ratón.
- `⚙` (o `⌃⌥⌘P`) abre Ajustes: copias, **skin** (Oscuro, Claro, Cian, Medianoche), **opacidad** y salir.
- Si solo hay una copia, o el portapapeles lleva archivos o imágenes, `⌘V` funciona como siempre.
- Sin icono en el Dock, sin ventana al arrancar, sin historial en disco.

## Privacidad

El historial vive **en RAM** y desaparece al cerrar. Solo se guardan las preferencias (copias, skin,
opacidad). No hay red, ni telemetría, ni cuentas, ni nube. Conviene cerrarla mientras se manejan
contraseñas: algunas apps no marcan bien los datos sensibles en el portapapeles.

## Compilar

Requiere Xcode Command Line Tools.

```bash
swiftc -swift-version 5 Core/HistoryStore.swift Tests/main.swift -o /tmp/copy4paste-history-test
/tmp/copy4paste-history-test
```

La app y el DMG los construye GitHub Actions en un runner macOS
(`.github/workflows/build-macos.yml`): tests, binario **universal** (Intel 10.15 + Apple Silicon 11.0),
prueba de interfaz, prueba de ejecución, firma, DMG en HFS+ y comprobación del DMG montado. Al empujar
una etiqueta `v*`, `release-macos.yml` publica la Release con el DMG, el ZIP y `SHA256SUMS`.

Notas de la web en [`docs/README.md`](docs/README.md).

## Licencia

MIT — consulta [LICENSE](LICENSE).
