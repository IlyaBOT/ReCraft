# ReCraft

[Build and launch](#manual-build-and-launch) | [Testing](#testing) | [Controls](#play)

ReCraft is a voxel sandbox built around raylib 1.4, GLFW 3.1.2, and a fixed-function OpenGL renderer. Its target is an i386 Mac running Mac OS X 10.6.8 with Intel GMA 950. Modern Windows, Linux and macOS builds are development/test hosts; a native Snow Leopard build and GPU run remain unverified.

The build version is stored in [VERSION](VERSION) and appears at the bottom left
of the main menu. `ReCraft --version` prints it without opening a window.
GitHub Actions builds Windows/Linux and macOS on the local Vesper runner.
See [CI, artifacts and host build instructions](docs/CI.md).

The game has 16 x 16 x 128 chunk columns, deterministic terrain for ReCraft saves, walking and flying controls, block interaction, crafting and chest/furnace inventories, survival health and mining, menus, video settings, a debug overlay, and reproducible benchmark scenes. Chunk block bytes use the Beta 1.7.3 ID range 0..96 with separate metadata nibbles; native `.rcg` version 1 saves are migrated on load to version 2. Minecraft Beta 1.7.3 `level.dat` and existing McRegion `.mcr` chunks can also be opened for play. Edits to loaded Beta chunks are written back in McRegion format. Missing Beta chunks are not generated yet. Some Beta blocks still use proxy geometry and behavior, and ReCraft's own terrain generator is not Beta-compatible. The per-block status is in the [Beta compatibility matrix](docs/BETA_COMPATIBILITY_MATRIX.md).
The offline Minecraft Beta 1.7.3 protocol 14 client is experimental. It needs testing against a real compatible server; it does not authenticate to online servers. The server list queries modern status (protocol 47) for MOTD, favicon, population and ping; modern gameplay is not implemented. Beta entries show TCP reachability only. See [network support and limits](docs/NETWORK_PROTOCOLS.md).
The interface uses selected user-provided CoterieCraft Beta textures and an original bitmap font atlas. Effects and music use original OGG assets with Beta sound keys and a non-looping music schedule. See [asset sources](docs/ASSET_SOURCES.md).

The current [mechanics audit](docs/BETA_MECHANICS_AUDIT.md) distinguishes working,
partial and missing features. Day/night, weather, beds and saved mobs have a
local implementation; full Beta terrain generation, pistons, armor and lightning
remain unfinished. Hostile spawning/pathfinding, skeleton arrows, creeper/TNT
explosions, leaf decay, fire and Q item dropping now have local simulation.
Instant Beta bows/projectile
arrows, jukebox discs 13/cat, legacy rails and three minecart variants now have
local simulation and vanilla NBT persistence. See [transport, fixes and remaining
differences](docs/BETA_TRANSPORT.md).
See [the events/profile update and remaining differences](docs/BETA_EVENTS_AND_PROFILE.md).

## Manual build and launch

### Get the project

Install Git, then open a terminal (PowerShell on Windows):

```sh
git clone https://github.com/IlyaBOT/ReCraft.git
cd ReCraft
```

If you already have the project, open a terminal in its root directory instead.
Run the commands below from that directory.

### Windows (PowerShell)

Install MSYS2 in `C:\msys64`. Install the build tools once:

```powershell
& C:\msys64\usr\bin\pacman.exe -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-openal mingw-w64-ucrt-x86_64-zlib
```

Build and start the game:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
cmake -P tools/fetch_dependencies.cmake
cmake -S . -B build/windows-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build/windows-ucrt --parallel 2
.\build\ReCraft.exe
```

For later launches, use the same PowerShell window, or run the `PATH` line again
before `.\build\ReCraft.exe`; the game needs DLLs from MSYS2 UCRT64.
If MSYS2 is installed elsewhere, replace `C:\msys64` in these commands.

### Linux / modern macOS

**Ubuntu/Debian:** install the build tools and libraries once:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake libgl1-mesa-dev libopenal-dev zlib1g-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxxf86vm-dev
```

**Modern macOS:** install Xcode command line tools with `xcode-select --install`
and install CMake 3.13 or newer. The system OpenGL/OpenAL frameworks are used.

Build on either system:

```sh
cmake -P tools/fetch_dependencies.cmake
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel 2
```

Start on **Linux**:

```sh
./build/ReCraft
```

Start on **macOS**:

```sh
open build/ReCraft.app
```

**Snow Leopard 10.6 / i386:** use the separate
[legacy build instructions](docs/SNOW_LEOPARD_BUILD.md).

### Rebuild after changes

Run only `cmake --build build/windows-ucrt --parallel 2` on Windows, or
`cmake --build build/host --parallel 2` on Linux/macOS, then start the game again.
The dependency script downloads the pinned raylib/GLFW sources automatically;
you do not need to copy them manually. An internet connection is needed for
the first download. If the pinned sources are already present, skip the fetch
command; it refuses to overwrite local dependency edits.

The executable and runtime assets are placed in `build/`; CMake files stay in
`build/windows-ucrt/` or `build/host/`. Put worlds in `build/saves/`.
On macOS, worlds and assets sit beside `ReCraft.app`.
Rebuilding preserves saves; do not delete the whole `build/` directory to clean
the compiler output. Resource lookup works independently of the current
working directory. See [runtime layout](docs/RUNTIME_LAYOUT.md) and
[CI and packaging](docs/CI.md) for advanced options.

## Play

- WASD and mouse: move and look; Space: jump; Ctrl: sprint.
- Left/right mouse: attack/break and interact/place. Number keys 1-9 and the
  mouse wheel select the hotbar slot.
- F: fly in creative worlds. Shift descends while flying.
- E: open the inventory. Survival has 36 slots and a 2 x 2 crafting grid.
  Left click moves/merges stacks; right click splits a stack or places one item.
  Creative opens a scrollable Beta block/item catalogue: select a hotbar slot,
  then click an item. The mouse wheel and right scrollbar browse the catalogue.
- Esc: pause; F2: save a screenshot in `screenshots/`; F3: measurements and renderer state.
- While connected, T opens chat, Enter sends it and Esc closes the draft.
  Hold Tab for self and nearby named players. Beta has no global roster or
  per-player ping packet, so unavailable ping is shown as `--`.
- Place a sign (item 323) against a solid material to open its four-line editor.
  Up/Down or Enter changes the active line; Done/Esc saves the text. Signs have
  16 standing directions or four wall attachments, and persist in native/Beta
  TileEntity data. Multiplayer waits for the server to confirm placement.
- Options: edit Player Name (1-16 letters, digits or `_`); it is saved in
  `config/options.txt` and used by the offline protocol 14 connection.
- Q: throw one selected item, preserving its metadata and durability.
- Main menu: the nickname button under the player opens Profile. Choose the
  pack skin, Classic Steve, or a local 64x32/64x64 PNG. Optional Microsoft
  device-code login uses the public application ID in `MICROSOFT_CLIENT_ID`;
  [setup and AADSTS50020 instructions](docs/MICROSOFT_ACCOUNT.md).
- Right click with a bow and arrows in inventory to fire instantly (Beta has
  no charging). Right click a jukebox with disc 13/cat to insert, again to eject.
- Place cart items on rails. Right click a normal cart to ride; Shift exits.
  Chest carts open 27 slots; right click a furnace cart with coal to fuel it.

The local simulation runs at 20 Hz and interpolates the camera between ticks.
Creative placement leaves stack counts unchanged; survival placement consumes a
block. Survival mining uses Beta hardness, tool speed, harvest and durability
rules, with visible cracks, durability bars and collectible drops. The item-break
sound plays only when a worn tool actually breaks. Workbenches open 3 x 3
crafting (151 vanilla recipes); furnaces smelt using fuel/input/output slots.
Chests store 27 slots or 54 in a valid adjacent pair and drop their contents
when broken. Health, damage, air and death/respawn are present. Food heals
immediately as in Beta; buckets collect sources and place water/lava.
Water/lava use scheduled 5/30-tick updates, sloping surfaces and side faces.
Pending updates, item entities and container contents survive saving. Redstone
supports torch inversion/burnout, wire steps, weak/strong power, levers,
20-tick stone buttons and directional 2/4/6/8-tick repeaters; other mechanisms
remain incomplete. Beds advance the night after 100 sleep ticks and save a
respawn point. Day/night and rain/snow use the Beta world clock and climate.
Peaceful/Easy/Normal/Hard are selectable in Options. Saved animals and monsters
have bounded local simulation; cows provide milk, sheep can be sheared and
chickens lay eggs. Zombies, skeletons, spiders and creepers spawn in darkness,
pursue the player, use their Beta attacks and despawn at distance. Search and
simulation work are bounded; full Java AI/collision parity remains unverified.
Fire spreads on scheduled ticks, stationary lava ignites flammable neighbors,
leaves decay after losing their log connection, and primed TNT chains explosions.
See [the gameplay milestone and checks](docs/GAMEPLAY_PARITY.md).
Minecraft Beta worlds load their
player position and inventory from `level.dat` and now save those fields back
through a gzip temporary file. Reads fall back to `level.dat_old` when the primary
is missing or corrupt; writes rotate the previous valid primary into that file.
The first write also retains `level.dat.recraft.bak`.
Unrelated NBT, including armor slots, is preserved. Old ReCraft sidecars are
left on disk but no longer override Beta player data.
See [save recovery and its tests](docs/BETA_SAVE_RECOVERY.md).
Make a backup before editing a Beta world. To open a particular save directly,
use `--world "New World"` (the directory name under `build/saves/`).

The video screen exposes distance, fog, lighting, brightness, leaves, reduced transparency, VBO
mode, its storage budget, greedy meshing, mipmaps, frame rate/VSync, menu blur, and
rebuild budgets. Features without an implementation are shown disabled. The
VBO and mipmap controls are also disabled when the active OpenGL context lacks
the required capabilities. The
legacy defaults prioritize
small geometry and a pixel look; details are in [performance](docs/PERFORMANCE.md).

Smooth Lighting and Menu Blur default to OFF and are independent of Fast/Fancy.
Smooth lighting interpolates baked vertex colors across block/chunk boundaries.
Esc/options show the world behind the menu. Singleplayer pauses its simulation
and caches the background; multiplayer continues updating and rendering.
Blur is a small fixed-function texture pass, without FBOs or shaders.
See [implementation and verification notes](docs/CREATIVE_LIGHTING_PAUSE.md).

Fast leaves retain exterior cutout faces. Fancy uses the same crisp, fully opaque
leaf pixels and renders internal faces; it adds no blur or translucent blending.
Reduced Transparency defaults to OFF. When enabled, leaves, door windows and
portals are opaque; glass, water, ice, cobwebs and bed-leg silhouettes retain
their essential transparency. All modes use the existing fixed-function passes.
Chat/sign text also uses the original Minecraft 1.5.2 Unicode bitmap pages,
loaded as needed with nearest filtering. Characters outside the supplied BMP
glyphs fall back to `?`; bidirectional text and Arabic shaping remain unfinished.

### Texture packs and languages

Put a Beta texture pack folder or ZIP in `build/texturepacks/`, then choose
**Options → Texture Packs**. Compatible newer texture packs can go in
`build/resourcepacks/`. Missing textures use the bundled pack. Selecting a pack
reloads textures in the current world and saves the selection in
`build/config/options.txt`. Modern shaders, models and sound-pack JSON are not
supported; animated texture strips currently use their first frame.

The globe button beside Options opens the Minecraft 1.5.2 language list.
Language selection is saved in the same config. This is the translation
foundation: original menu captions are translated, while ReCraft-specific text
and some gameplay labels still use English.

Windows/Linux windows can be resized and maximized. Mouse sensitivity,
inversion and FOV are in Options, with the nickname field in the center.
The main menu shows the version from `VERSION`, source revision and UTC build
date. See [this stage's changes and remaining limits](docs/BETA_GAMEPLAY_UI.md).

## Testing

Testing is optional for a normal build and launch. Run these commands from the
project root after building. Windows commands need the UCRT64 `PATH` set above.

### Automated tests

**Windows (PowerShell):**

```powershell
ctest --test-dir build/windows-ucrt --output-on-failure --timeout 60
```

**Linux / macOS:**

```sh
ctest --test-dir build/host --output-on-failure --timeout 60
```

Renderer tests need a graphical session. On headless Ubuntu/Debian, install
`xvfb` and `xauth`, then run:

```sh
xvfb-run -a ctest --test-dir build/host --output-on-failure --timeout 60
```

### Smoke run and GUI screenshots

```powershell
.\build\ReCraft.exe --smoke-test --no-audio --frames 120 --capture build/smoke.png
```

For Linux, replace `.\build\ReCraft.exe` with `./build/ReCraft`; for macOS, use
`./build/ReCraft.app/Contents/MacOS/ReCraft`.

These previews seed **in-memory** smoke worlds only; persistent worlds never
receive sample inventory or fixture blocks:

```powershell
.\build\ReCraft.exe --smoke-test --screen player --no-audio --frames 40 --capture build/player-inventory.png
.\build\ReCraft.exe --smoke-test --screen crafting --no-audio --frames 40 --capture build/crafting.png
.\build\ReCraft.exe --smoke-test --screen furnace --no-audio --frames 40 --capture build/furnace.png
.\build\ReCraft.exe --smoke-test --screen large-chest --no-audio --frames 40 --capture build/chest.png
.\build\ReCraft.exe --smoke-test --screen blocks --no-audio --frames 50 --capture build/blocks.png
.\build\ReCraft.exe --smoke-test --screen materials --fancy-leaves 1 --no-audio --frames 40 --capture build/materials.png
.\build\ReCraft.exe --smoke-test --screen materials --fancy-leaves 1 --reduced-transparency 1 --no-audio --frames 40 --capture build/materials-reduced.png
.\build\ReCraft.exe --smoke-test --screen sign-edit --no-audio --frames 40 --capture build/sign-editor.png
.\build\ReCraft.exe --smoke-test --screen multiplayer-demo --no-audio --frames 40 --capture build/multiplayer-status.png
.\build\ReCraft.exe --smoke-test --screen events --no-audio --frames 3 --capture build/beta-events.png
```

The `player` view shows the Survival panel, textured biped and 2 x 2 crafting.
Other views are `inventory` (Creative catalogue),
`chest`, `health`, `day`, `night`, `rain`, `snow`, `bed`, `mobs`, `chat` and `players`.
`events` shows animated fire, primed TNT and the four hostile mob models.
The multiplayer demo uses synthetic in-memory entries without remote queries;
`server_status_test` tests the actual exchange against a loopback server.
The `snow` view forces snow classification in its transient fixture.
Smoke views ignore live gameplay movement; GUI hover and the inventory player
preview still follow the cursor. Normal interactive play remains available from the menu.

### Performance benchmarks

```powershell
.\build\ReCraft.exe --benchmark bench_flat --no-audio --frames 600 --distance 4 --csv build/flat.csv
.\build\ReCraft.exe --benchmark bench_stream --no-audio --frames 600 --distance 8 --csv build/stream.csv
```

See [BENCHMARK.md](docs/BENCHMARK.md) for scenes, CSV columns and measurement
limits.

## Technical documentation

- [Architecture and file formats](docs/ARCHITECTURE.md)
- [Network protocols and current limits](docs/NETWORK_PROTOCOLS.md)
- [Original GMA 950 analysis](docs/OPTIMIZATION_NOTES.md)
