# copy4paste 1.1

El portapapeles con historial para Mac. Copia varios textos y elige cuál pegar con una tecla.

## Descarga

- `copy4paste-macOS-universal.dmg` — instalador universal (**Intel 10.15 Catalina+** y
  **Apple Silicon 11 Big Sur+**), DMG en HFS+.
- `copy4paste-macOS-universal.zip` — la misma app comprimida.
- `SHA256SUMS` — hashes para comprobar la descarga.

## Instalación

1. Abre el DMG y arrastra `copy4paste.app` a **Aplicaciones**.
2. Primer arranque: **clic derecho → Abrir**.
3. Ajustes del Sistema → Privacidad y seguridad → **Accesibilidad** → activa `copy4paste`.
4. Copia dos textos distintos y pulsa `⌘V`.

## Novedades de la 1.1

- Botón **⚙** en el selector para abrir Ajustes.
- **Copias**: 4 por defecto, ajustable de 1 a 30.
- **Skins**: Oscuro, Claro, Cian y Medianoche.
- **Opacidad**: porcentaje editable (20–100 %), sin animaciones ni temporizadores.
- Selección y borrado seguros sobre la instantánea visible.
- Botón **Salir** en Ajustes.

## Aviso importante

El binario lleva **firma ad hoc** y **no está notarizado por Apple**: no hay cuenta Apple Developer
detrás del proyecto. Gatekeeper pedirá confirmación la primera vez. En Catalina o posterior:
clic derecho sobre `copy4paste.app` → **Abrir**. El permiso de Accesibilidad está ligado a la firma,
así que cada recompilación exige volver a marcarlo una vez.

## Verificación

- 18 comprobaciones del historial en `Tests/main.swift`.
- Prueba de interfaz (`--verify-ui`): una cruz por fila, cruz que recibe clics, texto que no bloquea
  el clic, selección que se mantiene y borrado conectado.
- Prueba de ejecución (`--verify-runtime`): historial leído del portapapeles real (dos textos, en orden)
  y estado del tap de teclado.
- Binario universal verificado con `lipo` y versiones mínimas con `vtool`.
- DMG montado en CI y binario comprobado dentro de la imagen.
- Prueba real del atajo `⌘V` con el selector flotante en un Mac con **macOS Catalina 10.15.7 (Intel)**.

Construido en GitHub Actions sobre `macos-15-intel` a partir de este repositorio. Sin dependencias
externas: solo frameworks del sistema.
