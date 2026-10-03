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
| `assets/gui/unknown_server.png` | Original `third_party/textures/misc/unknown_server.png` | Default server-list icon when no valid favicon is supplied |
| `assets/textures/item/sign.png` | Normalized Coterie `item/sign.png` | Preferred 64x32 standing/wall sign model texture |
| `assets/textures/entity/sign.png` | Original Beta 1.7.3 JAR `item/sign.png` | Sign model fallback when the preferred Coterie image is missing |
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

The shared [runtime allowlist](../assets/runtime_assets.txt) contains 30 PNGs
and 108 OGGs (94 effects/variants, 12 music tracks and two records), 138 files in total. All OGG copies were
compared byte-for-byte to the read-only installed instance. Textures already
in the normalized pack remain unchanged. Some sound-pool entries prepare
future actions; the [mechanics audit](BETA_MECHANICS_AUDIT.md) records which
actions are actually implemented. Sound samples load on demand; music uses
four small OpenAL buffers rather than decoding a full track into RAM.

Portal sound SHA-256:
`e99e597079059f33b1e926728c99ff145eb36097de77c0f66ee87236c0fddcda`.
The previously deployed 13 environment/mob PNGs already belonged to the
normalized pack; their source files were not replaced. The sign renderer now
uses the existing Coterie model texture first. The original sign fallback was
extracted from the official Beta 1.7.3 client JAR, verified against its published
SHA-1 `43db9b498cb67058d2e12d394e6507722e71bb45`. The JAR remains development
reference material; the runtime only needs the selected PNG.

Original sign fallback SHA-256:
`ccd4fa265f84ec55cb1f328b55b1a5b1e41e3dc8586cbae391e073a3a603c510`.
Original unknown-server icon SHA-256:
`b0b745c4573737e150db7b880fc01321a9e1f96180622b2c6200ea3944961690`.
The supplied default icon is 128x128 and is drawn with nearest filtering.
Received server favicons must be valid 64x64 PNGs; they stay in a bounded texture
cache and are not copied into the source tree or saved to disk. Missing or
invalid favicons use the original default icon.

CMake, legacy GNU make and CI packaging read the same allowlist and never copy
the full reference tree. Runtime paths never depend on `third_party/` or on
the downloaded reference JAR.

Transport assets: `textures/entity/arrows.png` (32x32) and `cart.png` (64x32)
are unchanged `item/arrows.png` and `item/cart.png` from that vanilla Beta JAR.
`records/13.ogg`, `records/cat.ogg` and `sounds/random/drr.ogg` are unchanged
copies of the read-only installed Beta resources. No additional discs are used.
Entity textures use nearest filtering; records use the existing bounded
Vorbis/OpenAL streaming system.

`textures/entity/boat.png` is unchanged Beta `item/boat.png` from that same
vanilla JAR. The ReCraft wordmark is drawn from pixel geometry in `ui.c`.

The installed Minecraft **1.5.2** instance was accessed read only. Its version
descriptor resolved the client at
`D:/MultiMC/libraries/com/mojang/minecraft/1.5.2/minecraft-1.5.2-client.jar`,
SHA-256 `dc0fa48951f61c12eafede5e46e248aa86ab86d1e4c28cd880c1d9c348ec44d6`.
Selected unchanged assets from it:

- `lang/languages.txt` and `.lang` files → `assets/lang/`;
- `font/glyph_sizes.bin` and 222 `font/glyph_*.png` pages →
  `assets/fonts/unicode/` (page filenames normalized to lowercase);
- `gui/gui.png` → `assets/gui/language.png`, using the original 20x20 globe
  at `(0,106)` and hover row `(0,126)`.

Original assets remain the property of Mojang. The modern language list and
font do not change Beta gameplay, block IDs, NBT or network protocol.
