# Asset sources and provenance

The source `assets/` tree contains normalized files from the user-provided
CoterieCraft Beta 1.7.3 pack. The original wrapper directory was removed after
its 85 paths were mapped to `assets/gui/`, `assets/textures/`, or
`assets/metadata/`. The short `pack.txt` was reconstructed from its audited
visible text; its original exact bytes are unverified. The full mapping and
current hashes are in [ASSET_MIGRATION.md](ASSET_MIGRATION.md).
Only the files required by current code are deployed:

| Runtime path | Source material | Current use |
| --- | --- | --- |
| `assets/gui/widgets.png` | `gui/gui.png` | Buttons and hotbar |
| `assets/gui/background.png` | `gui/background.png` | Tiled dirt menu background |
| `assets/gui/icons.png` | `gui/icons.png` | Crosshair, hearts and air bubbles |
| `assets/gui/inventory.png` | `gui/inventory.png` | Survival panel, Creative panel border and slot recess |
| `assets/gui/items.png` | `gui/items.png` | Beta item sprites |
| `assets/gui/panorama.png` | `title/bg/panorama0.png` | Static main-menu panorama |
| `assets/textures/terrain.png` | `terrain.png` | Terrain atlas and item icons |
| `assets/gui/crafting.png` | `gui/crafting.png` | 3 x 3 workbench panel |
| `assets/gui/furnace.png` | `gui/furnace.png` | Furnace slots, flame and progress |
| `assets/gui/container.png` | `gui/container.png` | 27/54-slot chest panels |
| `assets/textures/mob/char.png` | `mob/char.png` | Remote player model and first-person arm |
| `assets/sounds/portal/portal.ogg` | Installed Beta instance `resources/newsound/portal/portal.ogg` (read only) | Portal ambience |
| `assets/fonts/ascii.png` | `third_party/textures/font/ascii.png` | Bitmap GUI font |

The renderer repacks selected 16 px terrain tiles into a 1024 px POT
atlas at startup, repeating each tile in a 64 px cell. Visible water/lava/portal
slots are animated on the CPU; no shaders or FBOs are used. Missing terrain imagery falls back to the procedural
atlas; missing optional GUI textures produce a checker and a log entry. No
runtime path refers to `third_party/`. Other normalized Coterie files remain
in the source tree for future features and are not copied to `build/assets/`.

The Coterie `pack.txt` identifies the pack but does not state a license. The
user supplied these materials for this local project. Check rights before
redistributing a build or the normalized media files. Audio other than portal ambience is generated
by ReCraft. Original reference code and assets in `third_party/` stay outside
the runtime dependency graph.

The Creative screen reuses the existing inventory atlas; no extra image is
required. Item sprite coordinates are explicit Beta registry values in
`src/game/creative.c` (including dye damage variants and music discs), not
`item_id - 256`. Block previews retain metadata and use terrain.png.

The runtime allowlist is twelve PNG files and one OGG file. Portal sound SHA-256:
`e99e597079059f33b1e926728c99ff145eb36097de77c0f66ee87236c0fddcda`.
The four added PNGs already belonged to the normalized Coterie pack and match
the migration manifest; they were not copied again from reference media.
