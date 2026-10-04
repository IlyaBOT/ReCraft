# ReCraft 0.2.1

Experimental client release targeting Minecraft Beta 1.7.3 behavior.

## Download and launch

- **Windows x64:** extract the ZIP, then start `ReCraft.exe` inside the extracted folder. All required non-system DLLs are included; MSYS2 is not required to play.
- **Linux x64:** extract the tar.gz and run `./ReCraft`. Install system OpenAL, zlib, X11 and OpenGL libraries.
- **macOS x64:** extract the tar.gz and open `ReCraft.app`. This Vesper build targets its installed macOS SDK; it is not the Snow Leopard/i386 build.

Keep the executable/app and `assets/` together. Existing worlds belong in `saves/` beside them. Back up Beta worlds before editing them with this experimental client.

## Changes in 0.2.1

- Fixed climbing redstone dust: its texture runs vertically on all four wall faces, faces are no longer incorrectly culled, and the top joins the upper wire. Checked against both Beta 1.7.3 and 1.5.2 RenderBlocks.
- Corrected repeater east/west texture orientation and dust corners, T junctions and crosses. Existing Beta circuit simulation remains unchanged.
- Restored arrows, boats, minecarts and falling/TNT entities that could be hidden by a shared 64-model limit. TNT has an opaque textured body with a white flash.
- Falling sand/gravel have tick-synchronised rendering and block AABB collision. Stacked columns settle without losing blocks, and saved falling entities resume correctly.
- Improved Microsoft sign-in diagnostics distinguish token denial from profile failures.

Includes the previous release's mechanisms, dispensers, note blocks, pistons, burning effects, profiles/skins/capes and experimental Microsoft sessions. See the README and compatibility documentation for remaining Beta differences. Exact terrain generation and a complete multiplayer implementation are still in development.

## Package verification

All three platform CI jobs must pass before publication. The Windows archive is scanned with Microsoft Defender. `SHA256SUMS.txt` verifies the downloads; each archive also contains per-file checksums and `BUILD_INFO.txt` with the source revision.

The Windows executable is not Authenticode-signed, and the macOS app is not Developer ID-signed or notarized. Antivirus scanning and checksums do not guarantee browser reputation or establish a trusted publisher. Download the platform archive from this release's **Assets**, rather than a temporary Actions storage link. The automatically generated source archives are not playable builds.

Free fan parody. Not affiliated with Mojang or Microsoft. Original game assets belong to their respective owners; see the included asset notices.
