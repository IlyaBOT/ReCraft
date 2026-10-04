# ReCraft 0.2.2

Experimental client release targeting Minecraft Beta 1.7.3 behavior.

## Download and launch

- **Windows x64:** extract the ZIP, then start `ReCraft.exe` inside the extracted folder. All required non-system DLLs are included; MSYS2 is not required to play.
- **Linux x64:** extract the tar.gz and run `./ReCraft`. Install system OpenAL, zlib, X11 and OpenGL libraries.
- **macOS x64:** extract the tar.gz and open `ReCraft.app`. This Vesper build targets its installed macOS SDK; it is not the Snow Leopard/i386 build.

Keep the executable/app and `assets/` together. Existing worlds belong in `saves/` beside them. Back up Beta worlds before editing them with this experimental client.

## Changes in 0.2.2

- Added correct Beta textures for gold, iron and diamond storage blocks, sponge and sandstone. Sponge intentionally does not absorb water.
- Added wooden trapdoor side placement, orientation/open metadata, support drops, selection/collision and redstone control.
- Added pumpkin/jack o'lantern facing textures and placement; jack o'lantern emits light 15.
- Added Beta fence joins and 1.5-block collision; wooden/cobblestone stair shapes, orientations, collision, inventory/held models and half-block player stepping.
- Snow supports all eight legacy metadata heights, Beta collision rules, support checks and block-light melting. Normal placement remains one layer; no modern snow stacking.
- Fixed the stone button inventory model that resembled a pressure plate. Its original Beta texture is stone.
- Added a safe generator for a flat McRegion world with all 97 IDs and 420 labelled state specimens, plus reader/writer and geometry regression tests. Run `python tools/create_block_lab.py build/saves/ReCraft_Block_Lab_022` from a source checkout.
- Fixed protocol-14 movement Y/stance ordering, position/look packet variants, teleport interpolation, terrain readiness and disappearing server chunks. Added local commands (`/tp`, `/gamemode`, `/time`, `/weather`, `/seed`); multiplayer commands are forwarded to the server.

Includes 0.2.1's redstone texture, entity rendering and falling-block fixes. See the README and compatibility documentation for remaining Beta differences: exact terrain generation, complete multiplayer, crop/cake/ladder behavior and spawner AI remain unfinished. Returning to a password server's previous location after authentication still needs a live recheck; no password was automated or logged.

## Package verification

All three platform CI jobs must pass before publication. The Windows archive is scanned with Microsoft Defender. `SHA256SUMS.txt` verifies the downloads; each archive also contains per-file checksums and `BUILD_INFO.txt` with the source revision.

The Windows executable is not Authenticode-signed, and the macOS app is not Developer ID-signed or notarized. Antivirus scanning and checksums do not guarantee browser reputation or establish a trusted publisher. Download the platform archive from this release's **Assets**, rather than a temporary Actions storage link. The automatically generated source archives are not playable builds.

Free fan parody. Not affiliated with Mojang or Microsoft. Original game assets belong to their respective owners; see the included asset notices.
