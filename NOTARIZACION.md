# Por qué macOS avisa al abrir la app y cómo se quita

Resumen para quien llegue nuevo: **la app no está notarizada porque no hay cuenta Apple Developer.**
No es un fallo del programa; es cómo funciona macOS con software de autor desconocido.

## Qué significa «firmado ad hoc»

macOS comprueba dos cosas antes de ejecutar un binario descargado de internet:

1. **Firma de código**: que el archivo no se haya modificado desde que se firmó. La nuestra es
   *ad hoc* (`codesign --sign -`): una firma válida, pero **sin identidad detrás**. Cualquiera puede
   hacer una igual, así que no dice nada sobre quién la publica.
2. **Notarización**: que Apple haya revisado el binario y le haya dado un billete
   (`stapler`). Eso exige una cuenta **Apple Developer Program** (99 USD/año) y un certificado
   *Developer ID Application*.

Sin el segundo paso, Gatekeeper avisa. El aviso se puede saltar a mano, no eliminar.

## Cómo se salta, según la versión de macOS

| macOS | Qué hacer |
|---|---|
| 10.15 – 14 | Clic derecho → **Abrir** → **Abrir**. Una vez por app instalada. |
| 15 o posterior | Intentar abrir, luego Ajustes del Sistema → Privacidad y seguridad → bajar al aviso → **Abrir igualmente**. |
| Cualquiera, si lo anterior falla | `xattr -dr com.apple.quarantine /Applications/copy4paste.app` |

`xattr` solo quita la marca de «descargado de internet» a ese archivo concreto. No desactiva Gatekeeper
para el resto del sistema y no pide permisos de administrador para archivos propios.

## Cómo activar la notarización (el día que haya cuenta)

El flujo de compilación ya lo hace solo; **no hay que tocar código**. Se necesitan cinco secretos en
`Settings → Secrets and variables → Actions` del repositorio:

| Secreto | Qué es |
|---|---|
| `MACOS_CERT_P12` | Certificado *Developer ID Application* exportado como `.p12`, en **base64** (`base64 -i cert.p12 \| pbcopy`). |
| `MACOS_CERT_PASSWORD` | La contraseña que se puso al exportar el `.p12`. |
| `APPLE_ID` | El correo de la cuenta Apple Developer. |
| `APPLE_APP_PASSWORD` | Contraseña específica de app, generada en <https://appleid.apple.com> → Seguridad. No es la del Apple ID. |
| `APPLE_TEAM_ID` | Identificador del equipo, en <https://developer.apple.com/account> → Membership. |

Con ellos, `build-macos.yml`:

1. Importa el certificado a un keychain temporal del runner.
2. Firma la app con `--options runtime --timestamp` (Hardened Runtime, obligatorio para notarizar).
3. Notariza el ZIP con `notarytool --wait`, hace `stapler staple` y lo valida.
4. Repite con el DMG, lo sella y comprueba con `spctl`.
5. Publica el resultado, además del `SHA256SUMS`.

Sin los secretos, el mismo flujo firma *ad hoc* y termina igual: es el estado actual y está probado.

## Alta de la cuenta, en corto

1. <https://developer.apple.com/programs/> → Enroll → persona individual.
2. Verificación de identidad con el Apple ID y la tarjeta. Suele tardar entre 24 y 48 horas.
3. En el portal: **Certificates → + → Developer ID Application**. Se genera un CSR desde
   *Acceso a Llaveros → Asistente para certificados* y se sube.
4. Descargar el `.cer`, instalarlo en el llavero, exportarlo junto a su clave privada como `.p12`.
5. Crear los cinco secretos y volver a etiquetar (`git tag -f v1.1.0 && git push -f origin v1.1.0`).

Ese último paso reconstruye la Release ya notarizada con el mismo código y el mismo número de versión.
