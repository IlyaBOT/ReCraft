ReCraft development preview, targeting Minecraft Beta 1.7.3 behavior.

## Download and launch

- **Windows x64:** extract the ZIP, then start `ReCraft.exe` inside the extracted folder. All required non-system DLLs are included; MSYS2 is not required to play.
- **Linux x64:** extract the tar.gz and run `./ReCraft`. Install system OpenAL, zlib, X11 and OpenGL libraries.
- **macOS x64:** extract the tar.gz and open `ReCraft.app`. This Vesper build targets its installed macOS SDK; it is not the Snow Leopard/i386 build.

Keep the executable/app and `assets/` together. Existing worlds belong in `saves/` beside them. Back up Beta worlds before editing them with this experimental client.

## Changes in this preview

Legacy block mechanisms, dispensers, note blocks, pistons, creative catalogue corrections, falling blocks and TNT, burning effects, player profiles/skins/capes and experimental Microsoft session support. See the included README and project documentation for implemented behavior and remaining Beta differences.

## Package verification

All three platform CI jobs must pass before publication. The Windows archive is scanned with Microsoft Defender. `SHA256SUMS.txt` verifies the downloads; each archive also contains per-file checksums and `BUILD_INFO.txt` with the source revision.

The Windows executable is not Authenticode-signed, and the macOS app is not Developer ID-signed or notarized. Antivirus scanning and checksums do not guarantee browser reputation or establish a trusted publisher. Download the platform archive from this release's **Assets**, rather than a temporary Actions storage link. The automatically generated source archives are not playable builds.

Free fan parody. Not affiliated with Mojang or Microsoft. Original game assets belong to their respective owners; see the included asset notices.
