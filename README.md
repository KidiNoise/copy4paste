# copy4paste

Portapapeles ligero para Mac Intel (macOS 10.15 Catalina o posterior). Una ventana flotante
de ~290 px aparece al pulsar **⌘V** cuando hay dos o más textos copiados.

## Qué hace

- `⌘C` guarda los últimos **4 textos** (ajustable de 1 a 30 en Ajustes).
- `⌘V`: si hay más de un texto, selector compacto; la copia **más reciente siempre es la 1**.
  - `1`–`9` pega esa copia.
  - `↑`/`↓` mueve el sombreado, `Intro` pega.
  - `Retroceso` borra la copia seleccionada **sin pegar**.
  - `×` a la derecha de cada fila borra esa fila con el ratón.
  - `Esc` cierra sin pegar.
- `⌃⌥⌘P` abre Ajustes.
- Con un solo texto, o si el portapapeles lleva archivos/imágenes, **⌘V se comporta normal**.
- Sin ventana ni icono en el Dock al arrancar (LSUIElement). Sin demo.
- El historial vive **solo en RAM**; en disco solo queda el número de copias (UserDefaults).
- Al arrancar cierra instancias antiguas del producto (mismo bundle ID o ejecutable `copy4paste`).

## Instalar

1. Abre `copy4paste-macOS-Catalina.dmg`.
2. Arrastra **copy4paste** a la carpeta **Aplicaciones**.
3. La primera vez: clic derecho sobre la app → **Abrir** → **Abrir** otra vez.
   (Gatekeeper avisa porque el binario tiene firma *ad hoc*, no notarizada por Apple.)
4. En **Ajustes del Sistema → Privacidad y seguridad → Accesibilidad**, activa **copy4paste**.
   En algunas versiones de macOS también pide **Supervisión de entrada**.
5. Copia dos textos distintos y pulsa ⌘V: aparece el selector.

Si el atajo no responde tras una actualización, desactiva y vuelve a activar el permiso de
Accesibilidad: al cambiar la firma, macOS invalida el permiso anterior.

## Compilación

`./.github/workflows/build-macos.yml` compila en un runner **macOS Intel** y produce el DMG y el
ZIP. No hace falta compilar nada en el Mac del usuario.

```
swiftc -swift-version 5 -target x86_64-apple-macosx10.15 \
  -framework AppKit -framework ApplicationServices -framework CoreGraphics \
  Core/HistoryStore.swift Sources/main.swift -o copy4paste.app/Contents/MacOS/copy4paste
```

Pruebas que corren antes de empaquetar:

- `Tests/main.swift` — 18 comprobaciones del historial.
- `--verify-ui` — cruz presente y clicable, texto que no bloquea el clic, sombreado.
- `--selftest` — interceptación de ⌘V con 1 y 2 textos, filas, cruces, sombreado con flechas,
  borrado sin pegar, copia elegida al portapapeles y límites de la preferencia 1–30.

## Límites conocidos

- Firmado *ad hoc*: **no notarizado**. Sin cuenta Apple Developer no hay forma de evitar el aviso
  de Gatekeeper en la primera apertura.
- Compilado y probado en un runner **macOS 15.7.9 Intel** con destino 10.15. No se ha ejecutado
  en un Mac con Catalina real.
- El pegado dentro de apps concretas (Notas, Safari…) y el retorno de foco no se pueden probar
  sin una sesión gráfica con permisos de Accesibilidad concedidos.

## Licencia

MIT — consulta `LICENSE`.
