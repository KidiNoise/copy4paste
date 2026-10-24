# Why macOS warns you when opening the app, and how to make that go away

Short version for anyone new: **the app is not notarized because there is no Apple Developer
account.** It is not a bug in the program; it is how macOS treats software from an unknown publisher.

## What «signed ad hoc» means

macOS checks two things before running a binary downloaded from the internet:

1. **Code signature**: that the file has not been modified since it was signed. Ours is *ad hoc*
   (`codesign --sign -`): a valid signature, but **with no identity behind it**. Anyone can make one,
   so it says nothing about who publishes it.
2. **Notarization**: that Apple has reviewed the binary and stamped a ticket on it (`stapler`). That
   requires an **Apple Developer Program** account (99 USD a year) and a *Developer ID Application*
   certificate.

Without the second step, Gatekeeper warns. The warning can be bypassed, not removed.

## How to get past it, by macOS version

| macOS | What to do |
|---|---|
| 10.15 – 14 | Right-click → **Open** → **Open**. Once per installed app. |
| 15 or later | Try to open it, then System Settings → Privacy & Security → scroll to the notice → **Open Anyway**. |
| Any, if the above fails | `xattr -dr com.apple.quarantine /Applications/copy4paste.app` |

`xattr` only clears the «downloaded from the internet» flag on that one file. It does not disable
Gatekeeper for the rest of the system, and it needs no administrator rights for your own files.

## Enabling notarization (the day there is an account)

The build workflow already does it on its own; **no code changes are needed**. Five secrets are
required under `Settings → Secrets and variables → Actions` in the repository:

| Secret | What it is |
|---|---|
| `MACOS_CERT_P12` | *Developer ID Application* certificate exported as `.p12`, in **base64** (`base64 -i cert.p12 \| pbcopy`). |
| `MACOS_CERT_PASSWORD` | The password chosen when exporting the `.p12`. |
| `APPLE_ID` | The email of the Apple Developer account. |
| `APPLE_APP_PASSWORD` | App-specific password, generated at <https://appleid.apple.com> → Sign-In and Security. Not your Apple ID password. |
| `APPLE_TEAM_ID` | Team identifier at <https://developer.apple.com/account> → Membership. |

With those in place, `build-macos.yml`:

1. Imports the certificate into a temporary keychain on the runner.
2. Signs the app with `--options runtime --timestamp` (Hardened Runtime, mandatory for notarizing).
3. Notarizes the ZIP with `notarytool --wait`, staples the ticket and validates it.
4. Repeats with the DMG, staples it and checks it with `spctl`.
5. Publishes the result alongside `SHA256SUMS`.

Without the secrets, the very same workflow signs ad hoc and finishes just the same: that is today's
state, and it is tested.

## Signing up, in brief

1. <https://developer.apple.com/programs/> → Enroll → individual.
2. Identity verification with your Apple ID and a card. Usually 24 to 48 hours.
3. In the portal: **Certificates → + → Developer ID Application**. A CSR is generated from
   *Keychain Access → Certificate Assistant* and uploaded.
4. Download the `.cer`, install it in the keychain, export it together with its private key as `.p12`.
5. Create the five secrets and re-tag (`git tag -f v1.1.0 && git push -f origin v1.1.0`).

That last step rebuilds the Release already notarized, with the same code and the same version number.
