# Microsoft account and local skins

Main menu → the button under the player model → **Player Profile**.
The nickname, skin choice and imported PNG survive a normal restart. The nickname
field lives only in Player Profile. A signed-in Java profile locks it and draws
the name in gray; the same official name is used for protocol 14. Nicknames
remain 1–16 ASCII letters/digits/underscores for the legacy protocol. Pack skin
and Classic Steve are the two bundled presets. **Choose skin file** imports a
64×32 or 64×64 PNG into `config/player_skin.png`; the selected file is never
modified. The model supports classic four-pixel arms, mirrored Beta limbs,
headwear and separate 64×64 left limbs/outer layers, with nearest filtering.
Three-pixel Slim/Alex geometry is not implemented yet.

## Create the ReCraft application

The application owner does this once; players do not need Azure subscriptions.

### AADSTS50020 when opening Entra

A personal Microsoft account opening the Entra admin center can land in the
shared **Microsoft Services** tenant. That directory has no administrative
directory linked to the account. The error referring to **ADIbizaUX** is about
the administration portal, before creating a ReCraft application.

Microsoft's documented solution is:

1. Register an [Azure free account](https://azure.microsoft.com/free/) using
   the personal Microsoft account. Initial registration creates a tenant and
   makes its owner the administrator. Follow Azure's eligibility/identity
   verification steps; registration availability depends on the region.
2. Open [Azure Portal](https://portal.azure.com/). Under your profile,
   **Switch directory / Directories + subscriptions**, select your own
   directory, usually **Default Directory**, rather than Microsoft Services.
3. Search for **App registrations** in that directory.

If an Azure account already exists, first switch to its existing directory.
Free/trial customers cannot create *additional* Workforce tenants through
Entra's tenant creation screen. See Microsoft's
[AADSTS50020 diagnosis](https://learn.microsoft.com/en-us/troubleshoot/entra/entra-id/app-integration/error-code-aadsts50020-user-account-identity-provider-does-not-exist)
and [tenant creation rules](https://learn.microsoft.com/en-us/entra/fundamentals/create-new-tenant).

### Register a public desktop client

1. **App registrations → New registration**, name **ReCraft**.
2. Supported accounts: **Personal Microsoft accounts only**, or the option
   including both organizational **and personal Microsoft accounts**.
   ReCraft signs personal accounts in through the `consumers` authority.
3. Register; copy **Application (client) ID** from Overview.
4. In **Authentication → Advanced settings**, enable **Allow public client
   flows**, then Save. Device-code flow does not require a redirect URI.
5. Do not create a client secret for this public desktop client.

See [Microsoft app registration](https://learn.microsoft.com/en-us/entra/identity-platform/quickstart-register-app)
and [desktop public client configuration](https://learn.microsoft.com/en-us/entra/identity-platform/scenario-desktop-app-registration).

Application registration alone does not guarantee Minecraft Services access:
Microsoft/Xbox/Minecraft may reject an application that has not received the
required service approval. ReCraft reports that rejection. Another launcher's
client ID or saved account file is not used as a workaround.

### Request Minecraft Services access

Registering the Entra application is only the OAuth step. Use Microsoft's
[AppID review link](https://aka.ms/mce-reviewappid) for the separate Minecraft
Services application-access request. On 4 October 2026 this link redirected
to a Microsoft Forms page. The form's fields and availability are controlled
by Microsoft; approval cannot be granted by ReCraft.

For the ReCraft application, prepare:

- Application name: `ReCraft`.
- Application/client ID: `9da281e5-3c38-4e92-a792-a819af4b28ec`.
- Directory/tenant ID: copy **Directory (tenant) ID** from the application's
  Overview in your own Entra directory if the form asks for it. It is a different
  value from the client ID; do not create another application to obtain it.
- Project website/source: `https://github.com/IlyaBOT/ReCraft`.
- An honest description of the client and the requested service access.

If offered a request type, choose the new AppID approval request. Suggested
description (adapt it to the actual form):

> ReCraft is an open-source experimental legacy Minecraft-compatible client
> with its own C engine. We request Minecraft Services access for our own
> public application ID so Java Edition players can obtain their official
> profile and authenticate compatible multiplayer sessions. Sign-in uses
> Microsoft's device-code flow; the client does not collect account passwords.
> Account tokens are stored locally in the player's game directory.

Submit the request yourself and retain the approval response. A pending request
does not enable service access. After approval for this same client ID, retry
sign-in in ReCraft. If it still fails, use the stage/status diagnostic below.

Official builds include the public application ID from the source-root
`MICROSOFT_CLIENT_ID` file. It is not a secret and is built by both CMake and
the Snow Leopard Makefile. Changing that file changes subsequent builds.

To override the built-in application, create `config/microsoft_auth.json`
in the game's runtime directory:

```json
{"client_id":"YOUR-APPLICATION-CLIENT-ID"}
```

Alternatively set `RECRAFT_MICROSOFT_CLIENT_ID` to the public client ID before
launching. The application owner can share **only client_id** for integration;
passwords, refresh/access tokens and client secrets are not needed in chat.

## Sign-in implementation

**Sign in with Microsoft** starts a worker thread. The normal browser opens
`microsoft.com/link`; the profile screen shows the device code and a Cancel
button. ReCraft does not display a password form or run an embedded browser.
The flow is device code / refresh token → Xbox user token → Minecraft XSTS
token → `/launcher/login` → Java profile and active skin/cape. It respects polling
intervals, `authorization_pending`, `slow_down`, expiry and cancellation.
An expired/revoked refresh grant starts a new device-code flow. The official
profile name becomes the shared player/network nickname after successful login.

HTTPS uses a separate `curl` process with certificate verification enabled,
an HTTPS host allowlist, bounded responses and timeouts. Request bodies and
bearer tokens are supplied on its standard input, not its command line. Windows
uses `%SystemRoot%/System32/curl.exe`; macOS/Linux use `/usr/bin/curl`.
`RECRAFT_CURL` can name another absolute helper path. Nothing is downloaded or
installed automatically. On Snow Leopard the old system TLS/curl may be
incompatible with today's Microsoft endpoints; a compatible TLS helper and an
actual target-platform sign-in remain unverified. Offline play has no curl
dependency. Skin file selection uses the OS dialog, osascript, or zenity/kdialog.

### Account persistence

`config/accounts.json` uses Prism's version-3 account schema: active MSA account,
`msa-client-id`, MSA access/refresh tokens, `utoken`, `xrp-mc`, `ygg`, and profile
name/id/skin and active capes. ReCraft owns its own **single-account** file and does not open
Prism/MultiMC user files. Like that schema, tokens are stored as JSON, without
an invented encryption layer. Writes use an exclusive temporary file and
atomic replacement. POSIX permissions are 0600; Windows writes an explicit
current-user-only DACL. Failed writes retain the previous file. Sign out
removes this file and clears the in-memory tokens; an imported/cached skin
remains available as a local cosmetic.

Reference behavior was reviewed in
[LCE Emerald Launcher](https://github.com/LCE-Hub/LCE-Emerald-Launcher),
[Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) and
[MultiMC](https://github.com/MultiMC/Launcher). Their Qt/Tauri/browser frameworks
are not runtime dependencies of ReCraft.

## Verification and boundaries

`account_test` uses dummy tokens and mock service responses. It verifies device
polling/backoff, refresh rotation/fallback, Xbox/MC audiences and request bodies,
malformed/rejected responses, cancellation, config loading, atomic save/reload,
and sign-out revision updates. It does not contact Microsoft in CI. A manual
unauthenticated request through ReCraft's HTTPS helper returned the expected
Minecraft-profile HTTP 401 on Windows.

The supplied application ID was accepted by Microsoft's device-code endpoint
(HTTP 200, 900-second expiry) through the actual ReCraft worker/TLS helper.
This caught a 1045-character device code that exceeded the old 1024-character
buffer; the bounded buffer is now 8192 bytes, with an explicit regression test.
The probe was cancelled before authorization, without saving any account tokens.
An actual account sign-in and Minecraft Services application access remain
unverified. For a protocol 14 online challenge, the client now posts the
Minecraft token, official UUID (`selectedProfile`) and server ID to
`https://sessionserver.mojang.com/session/minecraft/join` on a worker thread.
It waits for HTTP 204 before sending the Beta login. An expired token refreshes
before joining. Offline `-` challenges do not contact the session service.
Actual interoperability requires a compatible server whose authentication also
uses the current session service; retired server-side checkserver URLs are not
repaired by the client. Modern gameplay/RSA/AES remain absent.

Active skin and cape download through the asset manager. The cape cache is
`config/microsoft_cape_<uuid>.png`; it is cleared when the profile has no active
cape. Beta already had a cape model, so no new shader path was needed. Visible
remote Beta players use public Mojang name -> UUID -> textures lookup on a separate
worker, with at most 64 session cache entries. The same fetch interface accepts
UUID directly for future protocols. Remote PNGs are kept in memory, never in
account files. Other clients use the player's official profile textures normally.
Slim/Alex geometry, full cape movement animation, online skin upload,
multi-account selection and modern gameplay remain follow-up work.

## Minecraft Services rejection after browser authorization

Browser authorization confirms the Microsoft OAuth step only. The client reports
the service HTTP status and the failing stage, distinguishes an explicit
invalid-app-registration response from service outages, and avoids printing raw
credential-bearing responses.
The `/launcher/login` body was checked against the current
[Prism implementation](https://github.com/PrismLauncher/PrismLauncher/blob/develop/launcher/minecraft/auth/steps/LauncherLoginStep.cpp).

If the message says **app registration rejected**, the server explicitly named
application registration; request/check approval for the public client ID through
[AppID review](https://aka.ms/mce-reviewappid). Entra public-client settings
alone are not that approval. An unclassified **Minecraft login HTTP 403** does
not prove that the account lacks Java Edition or that the app was rejected:
the failure occurred before fetching the Java profile. If approval has not been
requested, complete that step first. If approval is already confirmed, collect
the bounded diagnostic to distinguish service denial from another failure.

Profile failures are separate: **profile HTTP 404** means a Java profile was not
found (check ownership and profile creation); **HTTP 401** means the service
rejected the token; **HTTP 429/5xx** means rate limiting/service failure. Invalid
HTTP-200 token/profile responses are parsing errors, not ownership diagnoses.

From PowerShell in the extracted Windows game directory:

```powershell
.\ReCraft.exe 2> .\account-diagnostic.txt
```

Try sign-in, exit the client, and inspect only lines starting `ReCraft account:`.
They contain fixed stage/status classifications, JSON/non-JSON response type and
an allowlisted service error name; no raw replies, tokens, player UUIDs or names
are printed. For example `stage=minecraft_login http=403 diagnosis=unspecified`
preserves uncertainty, whereas `diagnosis=app_registration_rejected` records an
explicit provider explanation. Do not share `config/accounts.json`.

The reported 4 October 2026 screenshot showed the old generic token-exchange
HTTP 403. Its private service response was not inspected. The current
`/launcher/login` request matches Prism's `xtoken`/`PC_LAUNCHER` request; matching
that contract and passing mock tests do not prove this application's approval
or a successful live account login.
