# Beta level.dat recovery

The local Beta `SaveHandler.loadWorldInfo` reference tries `level.dat`, then
`level.dat_old` on missing/broken input. `saveWorldInfoAndPlayer` writes
`level.dat_new`, rotates the prior primary to `level.dat_old`, then installs
the new primary. ReCraft now follows that file selection and rotation.

`src/world/beta_level_io.c` centralizes read-only loading, capped at 2 MiB of
uncompressed NBT. Gzip header/CRC, full NBT structure and the root's `Data`
compound must be valid. Discovery, player inventory import and saving use
the same source selection. Failed reads leave caller outputs unchanged.

Saving retains the first **valid** source as `level.dat.recraft.bak`. If loading
recovered from `level.dat_old`, the damaged/missing primary does not replace
that valid backup. Subsequent normal saves rotate the previous primary.
Unrelated NBT is retained, including weather, extra entity tags and armor slots. Health/Air/Fire are updated
from the current player; missing Player, position, inventory or Time fields are
created when saving.
Player/time changes are restricted to `Data`, not similarly named tags elsewhere.

`beta_recovery_test` generates its own compact fixture and checks primary
preference, missing/corrupt primary, gzip CRC failure, missing `Data`, read-only
fallback, both files invalid, save recovery/rotation, preserved fields and a
world with no Player or Time. It checks vitals and inventory after writeback.
It runs in CI without distributing real user worlds. `beta_level_test` checks
the real world's player/inventory/time roundtrip on a temporary copy.

Remaining limits: session.lock ownership is not enforced yet; concurrent
Minecraft/ReCraft writers are not supported. Missing Beta chunks, exact terrain
generation, most entity AI and many block mechanics remain
unfinished. This change adds recovery, not complete Beta compatibility.

McRegion writes preserve unknown entity/block-entity/tick compounds. Chest and
furnace records, collectible Item entities and supported `TileTicks` are
rewritten from current state. Tick delays are relative to the save instant,
matching Beta's `i/x/y/z/t` records. Unsupported ticks retain their original
NBT. Pending records belong to their chunks; queue pointers are removed before
eviction and restored on reload. `gameplay_test` verifies remaining delays,
eviction and repeated terrain-only saves; `beta_region_test` writes a cloned
region and verifies its unrelated NBT plus chest/item roundtrips.
