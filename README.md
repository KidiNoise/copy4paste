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
  <a href="https://github.com/KidiNoise/copy4paste/actions/workflows/build-macos.yml"><img alt="Compilación" src="https://github.com/KidiNoise/copy4paste/actions/workflows/build-macos.yml/badge.svg"></a>
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
tus últimas copias; pulsas `1`, `2`, `3`… y sigues trabajando. Nativo de macOS, sin Electron, sin red y
sin historial en disco.

> **Plataformas:** macOS disponible hoy (Intel y Apple Silicon en un mismo DMG). Las versiones de
> **Windows** y **Linux** están en desarrollo; se publicarán como descargas aparte y no cambiarán nada
> de lo que ya funciona en Mac.

## Instalación

1. Descarga el **DMG** y ábrelo.
2. Arrastra `copy4paste.app` a **Aplicaciones**.
3. Abre la app: **clic derecho → Abrir** → **Abrir** en el diálogo.
4. Ajustes del Sistema → Privacidad y seguridad → **Accesibilidad** → activa `copy4paste`.
5. Copia dos textos distintos y pulsa `⌘V`.

### La app no está notarizada por Apple, y esto es lo que te vas a encontrar

Esto es importante y conviene decirlo sin rodeos: **el binario va firmado *ad hoc*, no notarizado.**
Notarizar requiere una cuenta de Apple Developer Program (99 USD al año) que este proyecto no tiene.
Consecuencia práctica: macOS desconfía del archivo la primera vez, aunque el código sea abierto y
cualquiera pueda compilarlo.

Qué pasa según tu versión:

- **macOS 10.15 a 14**: clic derecho sobre la app → **Abrir** → **Abrir**. Solo la primera vez.
- **macOS 15 o posterior**: puede que «clic derecho → Abrir» ya no ofrezca ese botón. Entonces:
  1. Intenta abrir la app y espera a que macOS la bloquee.
  2. Ajustes del Sistema → **Privacidad y seguridad**.
  3. Baja hasta el aviso sobre `copy4paste` → **Abrir igualmente** → Touch ID o contraseña.
- **Si aun así no arranca**, abre Terminal y quita la marca de cuarentena del archivo descargado:

  ```bash
  xattr -dr com.apple.quarantine /Applications/copy4paste.app
  open /Applications/copy4paste.app
  ```

  Es el mismo mecanismo que usa cualquier app descargada de internet; no desactiva Gatekeeper para el
  resto del sistema.

Después de la primera apertura, el aviso no vuelve a salir. El permiso de **Accesibilidad** sí hay que
volver a concederlo cada vez que se recompile la app, porque está ligado a la firma.

Si vendes o compartes este proyecto y quieres que desaparezca el aviso para todo el mundo, el camino
está preparado: una cuenta Apple Developer, un certificado *Developer ID Application* y cinco secretos
en el repositorio. El flujo `.github/workflows/build-macos.yml` firma con Hardened Runtime, notariza y
verifica solo, sin tocar código. Ver [NOTARIZACION.md](NOTARIZACION.md).

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

## Descargas

| Archivo | Qué es |
|---|---|
| `copy4paste-macOS-universal.dmg` | Instalador. Intel 10.15+ y Apple Silicon 11+, en un solo binario. |
| `copy4paste-macOS-universal.zip` | La misma app comprimida, por si el DMG da problemas. |
| `SHA256SUMS` | Huellas para comprobar que la descarga llegó entera. |

## Compilar

Requiere Xcode Command Line Tools en macOS.

```bash
swiftc -swift-version 5 macos/Core/HistoryStore.swift macos/Tests/main.swift -o /tmp/copy4paste-history-test
/tmp/copy4paste-history-test
```

La app y el DMG los construye GitHub Actions en un runner macOS
(`.github/workflows/build-macos.yml`), sobre el código de `macos/`: tests, binario **universal** (Intel 10.15 + Apple Silicon 11.0),
prueba de interfaz, prueba de ejecución, firma, DMG en HFS+ y comprobación del DMG montado. Al empujar
una etiqueta `v*`, `release-macos.yml` publica la Release con el DMG, el ZIP y `SHA256SUMS`.

## Estructura

```
macos/     app AppKit (Sources, Core, Tests, Info.plist, icono)
docs/      la web del proyecto
.github/   compilación y publicación de la versión de macOS
```

Cada plataforma vive en su carpeta y tiene su propio flujo de compilación. Cuando estén Windows y
Linux, se añadirán como `windows/` y `linux/`, con su `.github/workflows/build-<plataforma>.yml`, y
publicarán su archivo en la misma Release. Nada de lo que ya funciona en macOS se toca: son archivos
nuevos al lado.

Notas de la web en [`docs/README.md`](docs/README.md).

## Licencia

MIT — consulta [LICENSE](LICENSE).
