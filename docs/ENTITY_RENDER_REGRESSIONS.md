# Entity rendering, falling blocks and redstone fixes

## Reference

The installed Minecraft **1.5.2** client JAR was read only:
`D:/MultiMC/libraries/com/mojang/minecraft/1.5.2/minecraft-1.5.2-client.jar`,
SHA-1 `465378c9dc2f779ae1d6e8046ebc46fb53a57968`.
CFR inspection of `rg` (EntityFallingSand), `amt` (BlockSand), `bgf`
(RenderBlocks), `aoe` (BlockRedstoneWire), and the materials/placement check
confirmed the rules below. Reference output stays in ignored `build/reference/`;
the client does not require Java or a decompiler. Third-party assets were
inspected; no additional runtime assets are needed.

## Changes

- A shared 64-model quota could hide arrows, vehicles and falling/TNT entities
  appended after mobs. The caller's bounded list is rendered with distance and
  conservative sphere/frustum culling instead. Transport interpolation uses
  the same 20 Hz tick remainder as the player camera, not a separate frame timer.
  Reloaded entities initialise their previous position from NBT.
- TNT and falling blocks draw an opaque textured base with depth writes;
  the white TNT flash is a separate overlay. The next entity receives normal
  colour/alpha state. Fixed-function OpenGL and nearest textures remain in use.
- Falling sand/gravel use a 0.98 x 0.98 body, gravity **0.04 blocks/tick**, motion
  before **0.98** drag, and collision offsets in **Y, X, Z** order. Collision
  uses the existing block/piston AABBs. The old sampled collision test could
  start inside a newly landed block: a 16-sand column reproduced **9 placed
  blocks / 7 dropped items** before the fix, versus **16 / 0** afterwards.
- BlockSand's initial delay is now **2 ticks**, as requested for 1.5.2 physics
  (Beta's previous delay was 3). The source is removed only on the first entity
  tick; duplicate scheduled updates cannot spawn a second owner. Landing places
  a block if the target is replaceable and has support, or drops its item.
  Moving pistons postpone landing. Grass/dead bush/snow can be replaced;
  torches and flowers cause an item drop. Fluids and fire permit falling.
- The erroneous unconditional 100-tick expiry is gone. It applies only outside
  the world's vertical bounds; the general airborne limit is 600 ticks.
  Beta's **128-block** world height is retained. Local sand/gravel are covered;
  this is not an implementation of modern anvils or 1.5.2's distant, unloaded
  chunk instant-fall path. Existing ReCraft chunk streaming remains in control.
- `FallingSand` keeps the Beta `Tile` byte and standard Entity Pos/Motion lists.
  Optional vanilla `Data` and `Time` bytes preserve metadata and progress.
  Old Beta records without these fields remain readable; a save before the
  first entity tick and a save in mid-flight both resume without duplicating
  the source. Unknown NBT fields remain preserved.
- Repeater top UV uses the vanilla vertex table: metadata 0/2 were correct;
  metadata 1/3 now rotate in the correct direction. Placement, signal direction
  and the delay torch positions retain their legacy metadata meanings.
- Dust connects to **both ends** of either repeater state, never its sides.
  Cross/line texture and highlight crops produce the 16 possible connection
  masks: isolated small cross, one connection, straight pair, corner pair,
  T junction and full cross. Step connections to another level require dust.
  These requested 1.5.2 visual connections have their own query. Existing Beta
  electrical output geometry and timing are preserved; this change does not
  migrate saved circuits to the later redstone simulation.
- Climbing dust now follows the shared Beta/1.5.2 wall vertex table: texture U
  follows height, winding faces the lower dust on all four sides, and the top
  reaches 1.021875 blocks to join the upper wire. Previously the generic quad
  UV placed the stripe horizontally, and south/north wall winding pointed
  into the support block, so back-face culling could hide those faces.

## Regression checks

`transport_test` checks sand and gravel columns, the numerical fall trace,
falling beyond 100 ticks, first-tick source ownership, and NBT reload before
the first tick and during flight. Existing arrow/boat/cart/TNT tests remain.

`renderer_test` and Windows `renderer_gl11_test` check actual pixels for
arrow/cart/boat/TNT/sand, opacity against two backgrounds, a list exceeding
64 models, tick interpolation, and rendering in front of/behind a terrain
wall. CPU mesh checks cover all 16 dust masks, both repeater ends and states,
all four repeater directions and all four delays.
Vertical dust checks cover four wall orientations, powered/unpowered colours,
both texture passes, positive/negative chunk seams, an obstructed climb and
missing upper dust. A real-atlas pixel check renders all four walls with
back-face culling enabled and verifies that the stripe spans their height.
`renderer_test --wire-capture <path.png>` exports this four-wall check.
The terrain/entity pixel fixture closes its world before releasing the
renderer; cached meshes still refer to that renderer during destruction.

`mechanisms_test` retains the four-repeater delay-2 ring comparison, with and
without a dust branch, for 160 ticks. Tests use temporary fixtures; user saves
and the installed Minecraft directories are not edited.

Local validation: Windows UCRT64 Release built without new compiler warnings;
**28/28 CTest passed**, including NVIDIA OpenGL and software OpenGL 1.1.
The effects/boats/repeaters smoke captures were visually inspected after
launching the executable from `C:/Windows/Temp` with an isolated data directory.
All **1,213** existing `build/saves/` files retained their SHA-256 hashes.
A native Snow Leopard/i386 hardware run remains unverified.
