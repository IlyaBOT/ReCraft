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
| `assets/textures/mob/{pig,sheep,sheep_fur,cow,chicken,zombie,skeleton,spider,creeper}.png` | Normalized Coterie `mob/` files | Local and remote mob models |
| `assets/textures/terrain/{sun,moon}.png` | Normalized Coterie `terrain/` files | Fixed-function celestial quads |
| `assets/textures/environment/{rain,snow}.png` | Normalized Coterie `environment/` files | Bounded rain/snow batches |
| `assets/sounds/portal/portal.ogg` | Installed Beta instance `resources/newsound/portal/portal.ogg` (read only) | Portal ambience |
| `assets/sounds/{step,random,liquid,fire,ambient/weather,mob}/...ogg` | Installed Beta `resources/newsound/` and `resources/sound/` | Original sound pool variants; material/action and entity keys |
| `assets/music/{calm1..3,hal1..4,nuance1..2,piano1..3}.ogg` | Installed Beta `resources/music/` and `resources/newmusic/` | Non-looping background music with idle delays |
| `assets/fonts/ascii.png` | `third_party/textures/font/ascii.png` | Bitmap GUI font |

The renderer repacks selected 16 px terrain tiles into a 1024 px POT
atlas at startup, repeating each tile in a 64 px cell. Visible water/lava/portal
slots are animated on the CPU; no shaders or FBOs are used. Missing terrain imagery falls back to the procedural
atlas; missing optional GUI textures produce a checker and a log entry. No
runtime path refers to `third_party/`. Other normalized Coterie files remain
in the source tree for future features and are not copied to `build/assets/`.

The Coterie `pack.txt` identifies the pack but does not state a license. The
user supplied these materials for this local project. Check rights before
redistributing a build or the normalized media files. Original sound/music
files also retain their original provenance. Original reference code and assets in `third_party/` stay outside
the runtime dependency graph.

The Creative screen reuses the existing inventory atlas; no extra image is
required. Item sprite coordinates are explicit Beta registry values in
`src/game/creative.c` (including dye damage variants and music discs), not
`item_id - 256`. Block previews retain metadata and use terrain.png.

The shared [runtime allowlist](../assets/runtime_assets.txt) contains 25 PNGs
and 105 OGGs (93 effects/variants and 12 music tracks). All OGG copies were
compared byte-for-byte to the read-only installed instance. Textures already
in the normalized pack remain unchanged. Some sound-pool entries prepare
future actions; the [mechanics audit](BETA_MECHANICS_AUDIT.md) records which
actions are actually implemented. Sound samples load on demand; music uses
four small OpenAL buffers rather than decoding a full track into RAM.

Portal sound SHA-256:
`e99e597079059f33b1e926728c99ff145eb36097de77c0f66ee87236c0fddcda`.
The 13 newly deployed PNGs already belonged to the normalized pack; their
source files were not replaced. CMake, legacy GNU make and CI packaging read
the same allowlist and never copy the full reference tree.
