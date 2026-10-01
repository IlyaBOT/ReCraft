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
Unrelated NBT is retained, including weather, player health and armor slots.
Player/time changes are restricted to `Data`, not similarly named tags elsewhere.

`beta_recovery_test` generates its own compact fixture and checks primary
preference, missing/corrupt primary, gzip CRC failure, missing `Data`, read-only
fallback, both files invalid, save recovery/rotation and preserved fields.
It runs in CI without distributing real user worlds. `beta_level_test` checks
the real world's player/inventory/time roundtrip on a temporary copy.

Remaining limits: session.lock ownership is not enforced yet; concurrent
Minecraft/ReCraft writers are not supported. Missing Beta chunks, exact terrain
generation, tick scheduling, most entities/tile entities and mechanics remain
unfinished. This change adds recovery, not complete Beta compatibility.
