# Beta blocks and the 0.2.2 block laboratory

## Source and implemented behavior

Checked against the installed unmodified `minecraft-b1.7.3-client.jar`, read
only. Original classes: `uu` (registry), `xf` (sponge), `rd` (sandstone), `l`
(storage cubes), `ss` (stairs), `jr` (snow), `jw` (fence), `fc` (pumpkins), `oq`
(trapdoor), plus the mapped Beta `BlockButton`/`RenderBlocks` reference.
Decompiled development references remain under ignored `build/reference/`.
No runtime dependency on these files or the Minecraft installation was added.

| Block | ID | Behavior and representation |
| --- | --- | --- |
| Sponge | 19 | Full opaque cube, terrain tile 48, **no water absorption** in Beta. |
| Sandstone | 24 | Top 176, side 192, bottom 208; full solid cube. |
| Gold / iron / diamond storage | 41 / 42 / 57 | Full solid cubes; tiles 23 / 22 / 24 on all faces. Existing mining/crafting rules apply. |
| Wooden / cobblestone stairs | 53 / 67 | Metadata 0..3; two collision boxes and cached stair meshes; automatic player step up to 0.5 blocks. Beta drops the model block (planks/cobblestone). |
| Fence | 85 | Central post and two horizontal rails; joins adjacent **fences only**. Beta collision is the entire voxel footprint, height 1.5. |
| Pumpkin / jack o'lantern | 86 / 91 | Metadata 0..3 chooses front south/west/north/east; top 102, sides 118, faces 119 / 120. Jack emits light 15. Placement needs a normal cube below; no golems. |
| Wooden trapdoor | 96 | Side placement only; facing bits 0..1, open bit 4. Thickness 3/16; collision/selection follow the open panel. Right click and redstone toggle it, lost support drops it. No iron, upper-half or waterlogged trapdoors. |
| Snow layer | 78 | Metadata `m & 7` gives visual/selection height `(m + 1)/8`. Beta's unusual collision is absent for m<3 and exactly 0.5 high for m>=3. Normal placement/snowfall creates m=0; no modern layer stacking. Support removal/melting at block light >11 removes it without a drop. |
| Stone button | 77 | Original stone tile 1, not a separate pressure-plate tile. Wall mesh is 6x4x2 pixels (one pixel deep pressed), with no body collision. Its inventory model is a small button instead of a broad plate. Existing 20-tick pulse remains. |

There are only two stair materials in Beta 1.7.3. Sandstone/brick/stone-brick
stairs and upside-down/corner stair states belong to later versions.
Shape-based inventory, held and dropped block models share `beta_block_item_boxes`.
All new world states retain numeric IDs and metadata nibbles in RCC1 and McRegion.

The all-block audit also removed remaining unnamed render fallbacks for
bookshelves, spawners, crops, farmland, ladders, cake and the obsolete locked
chest. Their textures are mapped to the Beta atlas; farmland/cake/ladder have
their partial visual bounds. Beta bookshelves drop nothing, unlike later versions.
This does **not** complete crop growth/hydration/trampling, ladder climbing,
cake eating, spawner AI or the obsolete locked-chest store. Those remain TODO.
Exact Java entity movement/callback ordering also remains broader parity work.

## Create a separate flat world

Python 3.8+ is a development tool only:

```sh
python tools/create_block_lab.py build/saves/ReCraft_Block_Lab_022
```

The generator refuses to overwrite any existing directory. It writes a real
Beta `level.dat` (gzip NBT, version 19132) and `region/*.mcr` (zlib NBT sectors).
The flat platform contains **420 labelled specimens**, covering all 97 registry
IDs 0..96 and audited variants: wool colors, fluids, doors, beds, rails/slopes,
repeater delays, torch/button/lever attachments, eight snow heights, pumpkins,
stairs and open/closed trapdoors. Chests/furnaces/signs/jukeboxes have vanilla
tile-entity records; rails have neighbours and slopes have raised supports.
The separate connected fence sample is near spawn. `block-lab.csv` records
each specimen's expected ID, metadata and coordinates.

Open **ReCraft Block Lab 0.2.2** in Singleplayer, or:

```powershell
.\build\ReCraft.exe --world ReCraft_Block_Lab_022
```

This is a Beta save, so initially survival; use `/gamemode creative` for local
inspection. Resume/play activates simulation: fluids flow, rails/redstone
update and technical states such as a moving piston can change. ID 0 is an
empty pedestal, ID 36 is a moving-piston tile entity, and door/bed halves are
paired. These are not all available as standalone inventory items in vanilla.

A paused inspection view preloads the registry area and disables save writes:

```powershell
.\build\ReCraft.exe --world ReCraft_Block_Lab_022 --screen block-lab --no-audio --frames 30 --capture build/block-lab.png
.\build\ReCraft.exe --smoke-test --screen block-states --no-audio --frames 30 --capture build/block-states.png
```

The first command acquires the ordinary Beta session lock but leaves chunk and
level data unchanged. The second uses an in-memory scene. Neither fixture is
packaged in releases or committed to Git; existing user worlds are preserved.

## Verification

- `legacy_shapes_test`: complete render definitions/atlas mappings, cube tiles,
  all snow heights and collision threshold, stair directions/drops/movement,
  all four trapdoor placements, manual/powered open/close, lost support, fence
  collision and small button inventory bounds.
- `renderer_test` / Windows `renderer_gl11_test`: cached stair/fence/snow/
  trapdoor/button geometry, alpha and height checks, alongside existing UI,
  entity, redstone, atlas and GL state regression tests.
- `block_lab_test`: creates an exclusive temporary save, proves overwrite
  refusal leaves every file unchanged, loads all specimens through the real
  C McRegion reader, saves/reopens and checks IDs, metadata and sign text.
  Registered when a Python 3 interpreter is available; client builds do not
  require Python.
- Existing gameplay, crafting, mining, transport, mechanisms, native/Beta
  persistence and networking regression tests remain in CTest.

Local Windows run: 30/30 CTests passed, including software OpenGL 1.1. Runtime
captures of the new blocks and the real generated world were inspected.
CI verifies Windows/Linux/Vesper separately before publication; Vesper is a
modern x64 macOS test host, not proof of a Snow Leopard/i386 run.
