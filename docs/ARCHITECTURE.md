# Architecture

ReCraft uses raylib 1.4 for the platform window, input, texture drawing and
audio. The terrain renderer owns its OpenGL 1.1 fixed-function path. It uses
no shaders, FBOs, per-block draw calls or per-face OpenGL lights. Snow Leopard
10.6/i386 remains the compatibility target; modern hosts are development hosts.

| Area | Source | Responsibility |
| --- | --- | --- |
| Loop and orchestration | `src/main.c` | Fixed 20 Hz simulation, interpolated camera, input, container sessions, streaming and diagnostics |
| World | `src/world/` | 16 x 16 x 128 chunks, Beta IDs/materials, bounded cache, native/McRegion storage, block entities, scheduled updates, fluids and item entities |
| Player and items | `src/game/` | Collision, movement, vitals, inventory, crafting, mining and fixed-function player/held-item rendering |
| Terrain | `src/renderer/` | Hidden-face and greedy meshing, atlas, baked lighting, culling, draw layers and CPU texture animation |
| Menus | `src/ui/` | Bitmap text, textured controls, container panels, Creative catalogue and HUD |
| Network | `src/network/` | Nonblocking offline protocol 14 client, bounded packet decoding and server container transactions |
| Profile and account | `src/account/` | Optional background device/refresh sign-in, Xbox/MC exchange and private atomic account JSON; local skin stays in the asset manager |
| NBT | `src/nbt/` | Bounded NBT reader/writer used by Beta storage and native entity sidecars |
| Assets and paths | `src/assets/`, `src/util/game_paths.c` | Executable-relative game root, logical texture/sound IDs, caches and nearest filtering |

## Chunks and meshes

Each chunk contains 32,768 block bytes and three packed 16,384-byte nibble
arrays: metadata, block light and sky light. The block registry stores the
stock Beta IDs 0..96; metadata remains a separate raw nibble. Meshing removes
hidden faces and merges compatible surfaces into opaque, cutout and
translucent layers. Chunk rebuilding and GL uploads stay on the main thread.
VBOs are optional and budgeted; client arrays remain the fallback.

The 1024 x 1024 terrain atlas reserves a 64 x 64 cell per selected 16 x 16
tile, repeating the tile four times in either direction. Greedy rectangles
therefore stop at four blocks per axis. A block always covers exactly 16
texels: removing a neighbour can change the mesh rectangle without changing
texture scale or phase. Each 16-byte vertex stores short positions at 1/128
block precision, short UVs and RGBA8 baked light.

Workbench, furnace and chest faces use their Beta terrain tiles; adjacent
chests select the two halves of the double-chest texture. Normal and redstone
torches use narrow attachment-oriented prisms with cropped UVs. Cactus uses
an inset box. Plants use crossed cutout planes. Half slabs keep partial-height
mesh and collision bounds. Portal blocks use a thin double-sided plane.
These additions stay in cached chunk layers rather than adding per-block draws.

Water and lava use weighted corner heights, exposed side faces and flowing
top UVs. Uniform fluid interiors retain greedy merging. CPU animation updates
the visible water, lava and portal atlas cells with `glTexSubImage2D`; portal
frames are precomputed. There is no shader or framebuffer animation path.
Other block types can still have proxy geometry or incomplete behaviour; see
[the compatibility matrix](BETA_COMPATIBILITY_MATRIX.md).

## Inventory and simulation

`crafting.c` matches 151 numeric Beta recipes, including shaped offsets,
reflection, shapeless ingredients and metadata wildcards. The player grid is
2 x 2; the workbench grid is 3 x 3. Results are take-only and ingredient
consumption is atomic. The item registry supplies stack limits and durability.
`tools/ExtractBetaRecipes.java` reads these tables from an unmodified installed
Beta client during development; the game needs neither Java nor that JAR.

`block_entity.c` owns chest and furnace records. Single chests expose 27 slots;
adjacent valid pairs expose 54 in stable half order. An opaque block above
either half prevents opening, and placement rejects triple chests. Furnace
input, fuel and take-only output run on the simulation tick, with 200-tick
smelting, Beta fuel durations and a lit/unlit block change. Breaking a
container creates item entities for its contents.

`physics.c` uses a bounded, deduplicated due-time heap. Water schedules at
five ticks, lava at 30, normal falling blocks at three and redstone torches at
two. `fluid.c` handles decay levels, source joining for water, source removal,
downward flow, outlet search and water/lava reactions. `redstone.c` supplies
directional weak/strong power, support-block conductivity, wire steps,
lever/button and repeater delays. It does not implement a complete redstone
engine (pistons, plates, doors and other mechanisms remain pending).
Cactus has support checks and random growth. Sand
and gravel resolve their landing position without a falling-entity animation.
`ticks.c` retains unsupported tick NBT and saves supported remaining delays.
Queue references are detached before chunk eviction and restored after load.
Simulation only touches loaded chunks; multiplayer block state is server owned.

`environment.c` advances the 24,000-tick clock and Beta weather timers.
`climate.c` implements Java Random and Beta 2D simplex octaves for precipitation
classification; it is not a terrain generator. Roof height is cached per chunk
revision. `weather.c` draws two bounded fixed-function texture batches.
Daylight subtracts an integer from stored skylight when baking meshes; source
NBT light values remain unchanged. `bed.c` owns two-half placement, sleep,
dawn and saved respawn points. Singleplayer pause stops these simulation ticks.

`entities.c` persists collectible item stacks, including count and damage,
position, motion, age and health. Pickup preserves metadata and stack limits.
Unknown entity and block-entity compounds are retained as NBT rather than
being simulated. Player health, air, fire, fall damage, cactus damage and local
death/respawn are implemented. Food heals immediately and buckets operate on
source blocks; there is no modern hunger system. Mining uses extracted hardness, tool strengths
and harvest rules; its progress and durability run at 20 Hz.

`mobs.c` reads eight Beta mob types and rewrites their known fields while
preserving unknown NBT. Local melee, health/drops, simple movement and animal
interactions are bounded. Hostile natural spawning, cardinal A*, skeleton arrows,
creeper/TNT explosions, foliage and scheduled fire now use the existing world
and persistence APIs. Complete armor and exact Java simulation parity remain
pending. See [events and profile](BETA_EVENTS_AND_PROFILE.md).

`sound_policy.c` uses extracted StepSound keys, volumes and pitches. Sound
variants are cached by the asset manager and played through a 16-voice OpenAL
pool. `music_stream.c` uses the existing stb_vorbis decoder with four small
buffers and no looping. The idle music countdown advances at controller ticks,
stops while music is playing/muted, and uses Beta's random initial/inter-track
delays. `assets/runtime_assets.txt` is shared by both builds and packaging.

## Storage

The native `RCC1` format is ReCraft's format, not Minecraft's. Version 2 stores
Beta block IDs; version 1's 12 private IDs are validated and remapped on load.
Native chunk records and entity/tick `.rct` sidecars use temporary-file writes.
RCW1 world metadata v2 adds time/weather and reads v1; native player.txt v3
adds bed spawn coordinates and reads v1/v2.

The McRegion adapter reads existing `.mcr` chunks and updates block, metadata,
light, chest/furnace `TileEntities`, item/mob `Entities` and `TileTicks` lists while retaining
unrelated NBT. Missing Beta chunks are not generated. ReCraft's native
generator is deterministic but is not the original Beta generator.

`beta_level.c` updates position, rotation, motion, inventory, Health/Air/Fire,
Time, weather timers/flags, bed spawn and LastPlayed in gzip `level.dat`. Missing player fields can be created
instead of preventing Save and Exit. Unknown tags and armor slots are retained;
the first valid input is backed up and Beta's old/new file rotation is used.
See [recovery and format limits](BETA_SAVE_RECOVERY.md).

## Multiplayer

The offline protocol 14 client consumes server chunks, blocks, health, player
and inventory events. Network worlds start empty; no client terrain is
generated for absent server chunks. The resolver is asynchronous and network
work is polled from the main loop. Protocol 47 remains a separate unfinished
adapter.

Server windows use Open/Close Window, Set Slot/Window Items, Window Property,
Click Window and Transaction packets. Slot mapping covers player crafting,
workbench, chest and furnace layouts. Clicks carry the original slot stack and
a transaction number; accepted/rejected replies and subsequent server slot
updates control the local session. Server furnace progress is displayed rather
than locally simulated. Mining sends start, cancel and finish at the computed
dig time. These paths require interoperability testing with a real Beta server;
mock packet tests cover rejection/resync, furnace properties and same-dimension
respawn, but do not establish full server compatibility.

Remote players use the 64 x 32 skin atlas, six body parts, head rotation and a
walking pose. First person draws the skin arm and selected item/block with
fixed-function geometry. Other mob types still have simplified geometry and
retain some differences from original entity models and AI. Player previews
also support classic-sized 64 x 64 skins and outer layers; geometric mirroring
and atlas UV calculations are shared in `skin_geometry.h`.

## Creative, light boundaries and pause backgrounds

`creative.c` owns the Beta item icon table and metadata catalogue. `ui.c`
displays 9 x 5 items, a scrollbar and the hotbar. Only a local Creative player
can grant items. All container screens use the common bitmap text, slot and
cursor-stack helpers.

The Survival panel places its `Crafting` caption at Beta's source coordinates
86,16 and draws the existing 64 x 32 player skin/biped in the model window.
`player_inventory_draw()` uses an orthographic fixed-function pass, two small
lights and cursor-dependent pose. It clears depth only inside that window and
restores matrices, viewport, texture binding, lighting, depth and scissor state.
The transparent pack window receives an opaque black backing; assets are not
modified. Container captions use eight-pixel bitmap glyphs at atlas scale,
without shadows. Survival has no extra Inventory caption over the model.

Block light relaxation uses loaded chunk edges as boundary conditions and
revisits neighbours whose boundary values changed. Every propagation step
loses a light level. It never loads chunks recursively; physics batches flush
light once. Original McRegion light is retained on initial read. Sky light
still lacks complete Beta lateral propagation. Smooth lighting defaults OFF;
diagonal mesh invalidations are needed only when it is enabled.

`renderer/menu_background.c` copies the scene with OpenGL 1.1
`glCopyTexSubImage2D`. Singleplayer pauses simulation and retains its frame
after pending meshes settle. Multiplayer keeps network, simulation, streaming
and rendering active with zero menu movement input. Blur defaults OFF; when
enabled, it uses a view no larger than 512 pixels per axis and four bilinear
samples. UI is then drawn at full resolution. Resize/options invalidate the
cache.

See [PERFORMANCE.md](PERFORMANCE.md) for GMA 950 policy,
[BENCHMARK.md](BENCHMARK.md) for measurements and
[GAMEPLAY_PARITY.md](GAMEPLAY_PARITY.md) for the current gameplay milestone.
