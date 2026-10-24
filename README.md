# copy4paste — versión candidata 1.0 (código fuente)

Aplicación nativa y minimalista de portapapeles para Mac Intel con macOS 10.15 o posterior.

## Funcionamiento
- `⌘C` conserva los **últimos 4 textos**; ajustable entre 1 y 30.
- `⌘V`: si hay más de un texto, selector compacto con **última copia arriba**, fila sombreada y cruz independiente por fila.
- `1–9` elige, ↑/↓ + Intro navega, Retroceso elimina, `Esc` cancela.
- `⌃⌥⌘P` abre Ajustes.
- Si solo hay una copia o el portapapeles contiene archivos/imágenes, el pegado habitual no se modifica.
- Sin demo al arrancar, sin icono en Dock y sin almacenamiento del historial en disco.
- Al iniciarse pide cerrar de forma normal las versiones anteriores del mismo producto para evitar varios interceptores de `⌘V`.

## Compilación y distribución
La carpeta `.github/workflows` compila **una aplicación macOS Intel** en un Mac de GitHub Actions y publica `copy4paste-macOS-Catalina.dmg` y `.zip` como *artifacts* del flujo (no como Release). No requiere que el usuario compile Swift en su equipo.

**Limitación actual:** el binario generado tiene una firma *ad hoc* para pruebas; **no está notarizado por Apple**. Gatekeeper puede mostrar advertencias. Para distribuir al público con experiencia de instalación normal hace falta configurar un certificado Developer ID, Hardened Runtime y notarización de Apple con una cuenta Apple Developer. No recomendamos el artefacto unsigned como instalador final.

El código de la interfaz AppKit **todavía debe pasar la compilación en GitHub Actions y pruebas funcionales en macOS Catalina** antes de denominarse 1.0 estable.

## Seguridad y privacidad
- Todo el historial permanece en RAM durante la ejecución; las preferencias numéricas sí se guardan en `UserDefaults`.
- Se ignoran archivos, imágenes y tipos declarados como confidenciales/transitorios.
- El acceso global al teclado exige permisos de Accesibilidad y puede requerir Supervisión de entrada en versiones de macOS.
- No se envía información a servidores.
- Es recomendable cerrar temporalmente la utilidad al manejar contraseñas, porque algunas aplicaciones no señalan correctamente datos sensibles en el portapapeles.

## Licencia
MIT — consulta `LICENSE`.
