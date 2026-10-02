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
Unrelated NBT is retained, including extra entity tags and armor slots. Health/Air/Fire, time/weather and bed spawn are updated
from the current player; missing Player, position, inventory or Time fields are
created when saving.
Player/time changes are restricted to `Data`, not similarly named tags elsewhere.

`beta_recovery_test` generates its own compact fixture and checks primary
preference, missing/corrupt primary, gzip CRC failure, missing `Data`, read-only
fallback, both files invalid, save recovery/rotation, preserved fields and a
world with no Player or Time. It checks vitals and inventory after writeback.
It runs in CI without distributing real user worlds. `beta_level_test` checks
the real world's player/inventory/time/weather/bed roundtrip on a temporary copy.

Interactive play now writes Beta's eight-byte big-endian millisecond token to
`session.lock`. Saves and dirty chunk eviction check that token. Missing,
corrupt or replaced tokens block both region writes and level.dat rotation;
Save and Exit shows a specific error and keeps the in-memory world available.
Read-only discovery/tests never acquire a session. `beta_session_test` checks
the format and competing sessions; recovery/region tests also verify rejected
writes leave the cloned files unchanged. This matches Beta's advisory token
scheme, not an exclusive OS lock or a multi-file transaction.

Remaining limits: simultaneous writers between a token check and disk write
are not made transactional. Missing Beta chunks, exact terrain
generation, most entity AI and many block mechanics remain
unfinished. This change adds recovery, not complete Beta compatibility.

McRegion writes preserve unknown entity/block-entity/tick compounds. Chest and
furnace records, collectible Item entities, supported mobs and `TileTicks` are
rewritten from current state. Tick delays are relative to the save instant,
matching Beta's `i/x/y/z/t` records. Unsupported ticks retain their original
NBT. Pending records belong to their chunks; queue pointers are removed before
eviction and restored on reload. `gameplay_test` verifies remaining delays,
eviction and repeated terrain-only saves; `beta_region_test` writes a cloned
region and verifies its unrelated NBT plus chest/item roundtrips.
