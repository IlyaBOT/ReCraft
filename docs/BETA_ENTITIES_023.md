# ReCraft 0.2.3: multiplayer entities and partial-block lighting

## Reference checks

The installed Beta 1.7.3 client JAR and development-only decompiled references
were read without changing the Minecraft instances. The relevant classes are
NetClientHandler, EntityClientPlayerMP, Entity, EntityItem, EntityBoat,
EntityMinecart, World, EntityLiving, EntityCreature, EntitySkeleton, EntitySpider,
EntityCreeper and ModelSpider. Mapped source is also available in the
[Beta source reference](https://github.com/jacobo-mc/mc_b1.7.3_release/tree/main/1.7.3-LTS/src/minecraft/net/minecraft/src).
Runtime has no dependency on these files or third_party reference paths.

## Network synchronization

- AttachEntity confirms mounting. Self and other passengers track the vehicle;
  walking simulation stops while mounted. Boat seat X/Z offsets rotate with yaw.
- Protocol-14 riding packets carry motion and both original `-999` sentinels.
  Seated feet/eyes account for the vanilla player yOffset, not a standing offset.
- Item spawn preserves the stack ID/count/damage and signed velocity bytes.
  Gravity, ground collision, friction, ice and bounce are simulated in loaded
  terrain. Collect removes the entity; only server inventory packets grant items.
  Offline items already use vanilla `Item` Entity NBT and retain that format.
- Position interpolation runs every rendered frame. Remote mob interpolation is
  three ticks; boats/carts use their original longer interpolation periods.
  Walking amplitude follows actual displacement and decays to zero at rest.
- Nonblocking network processing has a four millisecond soft budget and the
  existing packet/chunk limits. One large packet can exceed the soft budget.
  F3 separates network processing and chunk application from rendering.
- Entity picking selects the closest hit in front of terrain. Held attack input
  is consumed while targeting a mob/vehicle, preventing block mining behind it.

## Movement and light

The player clips Y, X and Z against actual block collision boxes, then compares a
single 0.5-block step candidate with ordinary movement. Landing, diagonal steps
and low ceilings have dedicated regressions. Entity push impulses are separate
from keyboard movement; remote entity positions remain server-owned.

Slabs/stairs/farmland retain Beta light opacity. Brightness uses the maximum of
the upper and four horizontal neighbours, extending this rule to thin surfaces.
Inset faces sample their own voxel rather than an opaque ceiling above them.
Skylight now spreads horizontally under roofs and across loaded chunk boundaries.
Initial McRegion and network snapshot light remains authoritative; actual block
edits schedule local light updates.

Video Settings → Environment groups transparency, leaves, smooth lighting and
pause blur. **Red Torch Light** defaults to Vanilla. Red mode is a custom visual
option: a separate level-7 red channel propagates through loaded terrain, loses
light through water and stops at opaque blocks. It allocates a nibble buffer only
in affected chunks and is freed when disabled. The result is baked into existing
vertex colours during mesh rebuilds; there are no shaders, FBOs or per-frame light
floods. Sky/stronger white light keeps its normal colour. No colour data enters NBT.

## Mobs

Creative attacks now use ordinary Beta item damage: bare hand 1 HP, swords
4/6/8/10 HP by tier (gold 4), and the corresponding axe/pick/shovel values. Each
mob keeps its own health and hurt cooldown. Successful hits cause knockback;
rejected hits do not consume tool durability. Death lasts 20 ticks, and vanilla
HurtTime/DeathTime fields round-trip through NBT.

Idle mobs stop swinging their legs. Spider leg yaw/roll follows ModelSpider's
eight mirrored limbs; skeletons render their held bow. Idle looking/turning,
target turning, path refresh, skeleton ranged attacks, spider lunging and melee,
creeper fuse/explosion, daylight burning and ambient sounds use Beta policies.
Network mob AI and damage are decided by the server.

## Verification

- Release build and the full CTest suite, including OpenGL/client-array checks.
- New headless entity_state_test: riding pose, picking/occlusion, interpolation,
  idle animation, Item collision, step directions, diagonal landing at a negative
  chunk boundary, low ceiling, partial light and red-light removal/chunk crossing.
- Loopback network_test: fragmented AttachEntity, riding sentinels, Item stack
  and velocity, Collect, status, teleports, terrain readiness and unload.
- Renderer regression: a slab's inset top remains lit under an opaque ceiling.
- Config restart test for the red-light option; deterministic visual captures
  for mobs, Environment, and `--screen partial-light --colored-redstone 0|1`.
- Authorized live protocol check on goldenage.keii.dev: login accepted, return
  teleport received, 441 chunks and ready terrain. Over 35 seconds, network CPU
  averaged 0.070 ms with a 1.232 ms maximum on this Windows host. This measures
  packet processing, not rendered frame time or network latency.
- Existing save file hashes are compared before/after build and tests. Smoke
  previews seed only in-memory worlds. Packaging excludes saves/config/logs.

## Remaining limits

This is not a claim of complete, bit-identical Beta AI. Pathfinding has a 512-node
limit and two searches per tick for legacy CPUs; simulation/spawning remain bounded
to loaded nearby chunks. Retaliation among different mobs, all passive/special mob
behaviors and natural-spawn parity still need further work. Live riding could not
be tested in this session because no vehicles were nearby. Exact Beta generation,
the remaining compatibility-matrix gaps and modern gameplay are separate work.
The published macOS build runs on Vesper's current SDK; Snow Leopard/i386 requires
the documented legacy build and has not been hardware-tested in this session.
