# Architecture

ReCraft uses raylib 1.4 for the platform window, input, texture drawing, and
audio. The terrain renderer owns its OpenGL 1.1 fixed-function path. There are
no shaders, framebuffers, geometry LOD, per-block draw calls, or per-face
OpenGL lights.

| Area | Source | Responsibility |
| --- | --- | --- |
| Loop and orchestration | `src/main.c` | Fixed 20 Hz simulation, input, menu actions, chunk streaming, diagnostics and benchmarks |
| World | `src/world/` | 16 x 16 x 128 chunk data, Beta numeric block catalog, deterministic native terrain, bounded cache, native and McRegion save/load |
| Player and entities | `src/game/` | AABB collision, movement, raycast, settings, primitive fixed-function remote-entity geometry |
| Terrain | `src/renderer/` | Hidden-face and greedy meshing, atlas, fixed-function lighting, culling, draw layers |
| Menus | `src/ui/` | Integer-scaled screens, controls, HUD and background |
| Network | `src/network/` | Nonblocking offline Beta 1.7.3 protocol 14 client and bounded packet decoding |
| NBT | `src/nbt/` | Separate bounded NBT reader/writer; not the native local save format |
| Assets and paths | `src/assets/`, `src/util/game_paths.c` | Executable-relative game root, logical asset IDs, cached textures and nearest filtering |

Each chunk stores 32,768 block bytes plus three packed nibble arrays of 16,384
bytes each: metadata, block light, and sky light. A chunk is marked dirty on
changes. Meshing removes hidden faces and merges compatible faces, then keeps
consolidated opaque, cutout and translucent layers for the chunk. A single
terrain atlas is created once. The renderer uses VBOs when supported and within
its conservative upload budget, and client arrays otherwise. OpenGL calls stay
on the main thread.

The block bytes now hold the Beta 1.7.3 numeric IDs 0..96. The authoritative
ID/name catalog is `src/world/beta_blocks.def`; metadata remains a raw nibble.
The originally rendered block types have approximate visuals. Wool colors,
log species, spruce leaves and planks now draw selected Beta terrain tiles.
Several simple cube blocks, including ores, bricks, obsidian, netherrack and
glowstone, also use their Beta terrain tiles. Glowstone and lit redstone ore
have their static light emission; block update behavior remains absent.
Saplings, tall grass, dead bushes, flowers, mushrooms and reeds now emit
double-sided crossed planes in the chunk cutout layer; there is no draw call
per plant. Tall grass and fern have a fixed plains tint, not a biome tint.
Ray selection and the outline use the audited Beta bounds for these plants;
walking through them still uses the empty collision rule.
Normal torches use attachment metadata for their cached, narrow wall or floor
prisms with cropped atlas UVs. Ray selection uses the Beta bounds; local placement maps the clicked
face to metadata 1–5. Redstone torches have selection bounds but no geometry
or redstone behavior yet.
Half slabs use a 0.5-block cached mesh and body/selection bounds. Their four
Beta material variants reuse the same 512 px atlas; a neighboring full cube
shows only its exposed upper half. Double slabs use a full cube. Local placement
merges half slabs when their metadata matches through the block-state API;
the Creative catalogue includes all four slab variants. Drops and exact sky-light propagation remain
incomplete.
Other IDs currently fall back to generic solid or hidden proxies. Those proxies
do not implement Beta collision shapes or behavior. See the
[per-block matrix](BETA_COMPATIBILITY_MATRIX.md).

The native save format belongs to ReCraft. It stores world metadata and
compressed chunk records with validation; it is not a Minecraft world format.
`RCC1` chunk version 2 stores Beta block IDs. Version 1 used 12 private IDs;
the loader validates its checksum and remaps those IDs before exposing the
chunk, then marks it for migration on the next save.
Native saves are written through temporary files. The separate NBT module reads
Beta `level.dat` metadata, position and inventory. The McRegion adapter loads
existing `.mcr` chunks and writes modified block/data/light arrays while
preserving other chunk NBT. It does not generate missing Beta chunks.
`beta_level.c` rewrites player position/rotation/motion, inventory, Time and
LastPlayed in gzip `level.dat`, preserving other tags and keeping the first
original in `level.dat.recraft.bak`. Native ReCraft saves still use sidecars.
Entity/tile entity simulation and exact scheduled ticks remain pending.

The network client requests offline protocol 14 login and consumes server
chunks, blocks, entity events and inventory slots. It keeps incoming Beta
block IDs and metadata without projecting them onto the 12 visual proxies.
IDs outside the stock Beta registry are rejected. Network worlds start empty;
missing chunks are never filled with locally generated terrain. The visible
multiplayer hotbar is driven by server inventory slot packets, with primitive
fixed-function geometry for remote entities. The hostname resolver is
asynchronous and network work is polled from the main loop. Protocol 47 needs
its own adapter before it can be supported. Native compatibility, especially
entity and inventory behavior, remains to be tested against an actual server.

See [PERFORMANCE.md](PERFORMANCE.md) for GMA 950 policy and
[BENCHMARK.md](BENCHMARK.md) for reproducible measurements.

## Creative, light boundaries, and pause backgrounds

`game/creative.c` owns the Beta item icon table and deterministic catalogue,
including metadata variants. `ui.c` displays 9 x 5 items, a scrollbar, and the
nine hotbar slots. Only a local Creative player can grant items.

Block light relaxation uses loaded chunk edges as boundary conditions and
revisits only neighbours whose boundary values changed. Removal also converges
because every propagation step loses at least one level. It never loads chunks
recursively. Physics batches flush light once after block updates. Original
McRegion light is retained on initial read; absence of an unloaded chunk does
not imply that its emitters disappeared. Sky light still lacks Beta lateral
propagation. Diagonal mesh invalidations are needed only for smooth lighting.

`renderer/menu_background.c` copies the rendered scene with OpenGL 1.1
`glCopyTexSubImage2D`. Singleplayer retains the frame after pending meshes finish;
multiplayer keeps simulation/network/streaming/rendering active with zero menu
movement input. No-blur multiplayer draws directly. Blur renders a view no
larger than 512 pixels on either axis and draws four bilinear samples. Menu UI
is drawn afterward at full resolution. Resize/options invalidate the cache.
