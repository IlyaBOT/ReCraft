# ReCraft 0.2.3

Experimental client release targeting Minecraft Beta 1.7.3 behavior.

## Download and launch

- **Windows x64:** extract the ZIP, then start `ReCraft.exe` inside the extracted folder. All required non-system DLLs are included; MSYS2 is not required to play.
- **Linux x64:** extract the tar.gz and run `./ReCraft`. Install system OpenAL, zlib, X11 and OpenGL libraries.
- **macOS x64:** extract the tar.gz and open `ReCraft.app`. This Vesper build targets its installed macOS SDK; it is not the Snow Leopard/i386 build.

Keep the executable/app and `assets/` together. Existing worlds belong in `saves/` beside them. Back up Beta worlds before editing them with this experimental client.

## Changes in 0.2.3

- Fixed server-confirmed boat/minecart mounting, seated camera/passenger poses,
  riding motion packets, dismount and respawn cleanup.
- Added proper dropped Item entity stacks, velocities, collision, nearest texture
  rendering and server-authoritative collection. Offline Item NBT is preserved.
- Enabled multiplayer player/entity interpolation. Added F3 network timing and
  a soft packet processing budget; the client does not wait for server ticks.
- Fixed entity attacks falling through to block mining while holding the mouse.
  Added network entity push impulses and corrected vehicle picking bounds.
- Reworked half-block stepping with swept AABB clipping, combined horizontal
  movement and grounded landing checks, including diagonal/chunk-boundary cases.
- Fixed brightness sampling for slabs, stairs, farmland, wire and repeaters;
  added lateral skylight propagation and correct partial-face sampling below roofs.
- Added Environment settings and optional red torch light, disabled by default.
  Colours are baked into mesh vertices on the CPU; no shader or NBT extension.
- Fixed idle limb animation and spider legs; skeletons hold bows. Mob health and
  tool damage now apply in creative too. Added hit knockback, hurt/death rendering,
  vanilla transient NBT fields and closer Beta idle/look/attack behavior.
- User-confirmed password-server return teleport was verified by a live network
  probe. Existing saves were checked for unchanged contents.

See [verification details and remaining differences](BETA_ENTITIES_023.md) in the
source repository, or `docs/BETA_ENTITIES_023.md` inside the downloaded package.
Exact Beta terrain generation, exhaustive AI/pathfinding parity, every mob type,
online account/app approval and modern gameplay protocols remain unfinished.
No nearby boat/cart was available in the live check; riding is verified by packet
and simulation regressions, and still merits a manual server ride.

## Package verification

All three platform CI jobs must pass before publication. The Windows archive is scanned with Microsoft Defender. `SHA256SUMS.txt` verifies the downloads; each archive also contains per-file checksums and `BUILD_INFO.txt` with the source revision.

The Windows executable is not Authenticode-signed, and the macOS app is not Developer ID-signed or notarized. Antivirus scanning and checksums do not guarantee browser reputation or establish a trusted publisher. Download the platform archive from this release's **Assets**, rather than a temporary Actions storage link. The automatically generated source archives are not playable builds.

Free fan parody. Not affiliated with Mojang or Microsoft. Original game assets belong to their respective owners; see the included asset notices.
