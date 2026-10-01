# ReCraft

ReCraft is a voxel sandbox built around raylib 1.4, GLFW 3.1.2, and a fixed-function OpenGL renderer. Its target is an i386 Mac running Mac OS X 10.6.8 with Intel GMA 950. Modern Windows, Linux and macOS builds are development/test hosts; a native Snow Leopard build and GPU run remain unverified.

The build version is stored in [VERSION](VERSION) and appears at the bottom left
of the main menu. `ReCraft --version` prints it without opening a window.
GitHub Actions builds Windows/Linux and macOS on the local Vesper runner.
See [CI, artifacts and host build instructions](docs/CI.md).

The game has 16 x 16 x 128 chunk columns, deterministic terrain for ReCraft saves, walking and flying controls, block interaction, an inventory, menus, video settings, a debug overlay, and reproducible benchmark scenes. Chunk block bytes use the Beta 1.7.3 ID range 0..96 with separate metadata nibbles; native `.rcg` version 1 saves are migrated on load to version 2. Minecraft Beta 1.7.3 `level.dat` and existing McRegion `.mcr` chunks can also be opened for play. Edits to loaded Beta chunks are written back in McRegion format. Missing Beta chunks are not generated yet. Most Beta blocks still use placeholder geometry and behavior, and ReCraft's own terrain generator is not Beta-compatible. The per-block status is in the [Beta compatibility matrix](docs/BETA_COMPATIBILITY_MATRIX.md).
The offline Minecraft Beta 1.7.3 protocol 14 client is experimental. It needs testing against a real compatible server; it does not authenticate to online servers. The interface uses selected user-provided CoterieCraft Beta textures and an original bitmap font atlas. Sound remains synthesized. See [asset sources](docs/ASSET_SOURCES.md).

## Build

For Snow Leopard, follow [the native build recipe](docs/SNOW_LEOPARD_BUILD.md). For Windows UCRT64, install MSYS2 UCRT64 GCC, Ninja, OpenAL Soft and zlib, and CMake 3.10 or newer. Fetch the pinned sources described in [third_party/README.md](third_party/README.md) into `.deps/` or `third_party/`.
In PowerShell:

```powershell
& C:\msys64\usr\bin\pacman.exe -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-openal mingw-w64-ucrt-x86_64-zlib mingw-w64-ucrt-x86_64-ninja
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
cmake -P tools/fetch_dependencies.cmake
cmake -S . -B build/windows-ucrt -G Ninja -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build/windows-ucrt --parallel
ctest --test-dir build/windows-ucrt --output-on-failure
```

Keep the UCRT64 `bin` directory on `PATH` when running the executable; its
OpenAL and compiler DLLs come from that installation. The Windows executable,
runtime assets and user worlds are in `build/`. On macOS the executable is in
`build/ReCraft.app` and persistent data is beside the bundle in `build/`.
Resources are resolved from the executable location, independent of the shell
working directory. `--data-dir` overrides the entire game root.
See [runtime layout](docs/RUNTIME_LAYOUT.md).

## Play

- WASD and mouse: move and look; Space: jump; Ctrl: sprint.
- Left/right mouse: break/place the selected block. Number keys 1-9 and the
  mouse wheel select the hotbar slot.
- F: fly in creative worlds. Shift descends while flying.
- E: open the inventory. Survival uses 36 slots (click two slots to swap).
  Creative opens a scrollable Beta block/item catalogue: select a hotbar slot,
  then click an item. The mouse wheel and right scrollbar browse the catalogue.
- Esc: pause; F2: save a screenshot in `screenshots/`; F3: measurements and renderer state. While connected, T opens
  chat and Enter sends it.

The local simulation runs at 20 Hz and interpolates the camera between ticks.
Creative placement leaves stack counts unchanged; survival placement consumes a
block and broken blocks can be picked up. Sand/gravel falling, torch support
and water flow are local approximations; unsupported torches drop as items.
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

The video screen exposes distance, fog, lighting, brightness, leaves, VBO
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

## Reproduce a run

```powershell
build\ReCraft.exe --smoke-test --no-audio --frames 120 --capture build/smoke.png
build\ReCraft.exe --benchmark bench_flat --no-audio --frames 600 --distance 4 --csv build/flat.csv
build\ReCraft.exe --benchmark bench_stream --no-audio --frames 600 --distance 8 --csv build/stream.csv
```

The available scenes, controls, CSV columns and measurement limits are in
[BENCHMARK.md](docs/BENCHMARK.md). Implementation boundaries and file formats
are in [ARCHITECTURE.md](docs/ARCHITECTURE.md). Implemented protocol traffic
and its limits are in [NETWORK_PROTOCOLS.md](docs/NETWORK_PROTOCOLS.md). The
original GMA 950 analysis is preserved in
[OPTIMIZATION_NOTES.md](docs/OPTIMIZATION_NOTES.md).
