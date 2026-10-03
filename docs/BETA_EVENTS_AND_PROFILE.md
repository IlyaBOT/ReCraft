# Beta events, player UV and profile update

This update checked the project's existing loaders/simulation and the Beta
reference classes before changing behavior. The reference client JAR is
`b1.7.3`, SHA-1 `43db9b498cb67058d2e12d394e6507722e71bb45`.
Reference/download files stay in ignored development areas; runtime never reads
`third_party` or the installed Minecraft instances.

## Rendering and interface

- `skin_geometry.h` follows ModelRenderer/TexturedQuad: six face UV rectangles,
  0.1 texel inset, geometric X mirror plus reversed quad order. Arms use 40,16,
  legs 0,16; mirrored limbs retain the atlas's vertical direction. The same
  calculation serves world players, inventory/menu previews and the empty hand.
- Classic headwear uses 32,0 and an inflated head. Classic-sized 64×64 skins
  retain independent left limbs and outer layers. Skeleton limbs now use 2×12×2
  geometry, rather than the player's 4×12×4 dimensions.
- Buttons use GuiButton's two outer atlas halves, with source widths expressed
  in 20-pixel logical button-height units. Narrow buttons no longer repeat the
  center ornament or use four unscaled device pixels for the edges. Disabled,
  normal and hovered/pressed rows remain 46/66/86. Labels use centered bitmap
  text, shadow, Beta colors and nearest filtering.
- The main menu has a player model at the left and profile button beneath it.
  Version/build revision/date/target OS and architecture are at the bottom left;
  GitHub, the smaller free-fan-parody/assets notice and larger affiliation notice
  are aligned at the right. Backgrounds fill the viewport when its aspect ratio
  differs from the logical menu canvas.
- [Microsoft account/skin setup and limits](MICROSOFT_ACCOUNT.md) describes the
  new profile screen, local skin import, device-code login and account storage.

## Leaf decay

Log removal marks surrounding leaves within four blocks with metadata bit 8;
leaf removal propagates the check flag within one block. The random tick checks
six-connected leaf paths up to four steps from logs, clears the flag when
supported and otherwise removes the leaf. Required neighboring chunks must be
loaded; it never generates terrain or treats missing Beta chunks as air.
Decay drops a species-matched sapling at 1/20 probability. There are no later
apple drops or later-version permanent-leaf block states. Shears retain the
existing Beta leaf harvest rule.

## TNT and explosions

Block 46 has the correct side/top/bottom atlas tiles. Redstone, fire and the
Beta flint-and-steel mining action prime it. PrimedTnt uses gravity/friction,
ground bounce, an 80-tick fuse with Beta's post-decrement termination, and
power 4. Exploded TNT gets a shortened 10–29 tick fuse. Chain explosions keep
new entities and dropped items even if the current entity is unlinked during
that tick.

Blast rays use the shell of a 16³ cube, 0.3-block steps, original resistance
scaling/attenuation and 30% block-drop probability. Entity exposure samples its
bounding box with voxel traversal and shape intersections; walls shield damage
and knockback. The player keeps horizontal knockback through subsequent input
ticks. Loaded blocks and containers are modified through the existing world
API. Bedrock/obsidian resist normal TNT; flowing lava's resistance differs from
stationary lava in this version.

PrimedTnt persists vanilla Entity NBT `id`, Pos/Motion/Rotation and **Fuse: Byte**.
Unknown tags remain intact. No private vanilla replacement format was added.
The main-loop import/export now converts awake Beta SP `Player.Pos[1]` using
the original `yOffset=1.62`; ReCraft internally keeps player feet coordinates.
The low-level NBT API still stores the supplied vanilla coordinates unchanged.
On import, saves with ReCraft's existing `level.dat.recraft.bak` can recover the
older feet-coordinate convention when the converted position intersects terrain
and the original position is clear. Ambiguous positions in open air retain
vanilla's convention. A saved sleeping player wakes beside the bed, clearing
its occupied bit, as in Beta's NBT reader. These cases have regression tests;
user save fixtures were not rewritten during the checks.
The entity render shows the fuse's swelling/flashing on the fixed-function path.

## Fire

Block 51 is non-solid, emits light 15 and uses a small CPU flame animation.
Age is legacy metadata 0–15; updates are scheduled every 40 ticks. The Beta
encouragement/burn tables, horizontal/vertical burn probabilities, upward spread,
rain extinction, support checks, eternal netherrack fire and TNT ignition are
implemented. Stationary lava uses Beta's 0–2-step upward random walk to ignite
nearby flammable material. Flint and steel place fire and wear in Survival;
left-click extinguishes adjacent fire without harvesting it as a cube. Original
`fire.fire`, `fire.ignite`, `random.fuse` and `random.old_explode` sounds use the
existing resource/audio manager. Fire metadata and scheduled updates use the
existing native/Beta persistence.

The visual fire mesh currently uses crossed planes; the full Beta wall-attached
fire geometry and smoke/explosion particles remain unfinished. A pack's explicit
modern fire tile override disables the procedural flame slot.

## Hostile mobs and Q

- Zombies pursue and attack the player, use their Beta movement/damage values,
  and burn in sunlight. Skeletons fire real Arrow entities at 0.6 blocks/tick,
  inaccuracy 12, and a 30-tick attack cooldown; their arrows cannot be picked up.
- Spiders acquire targets in darkness, can forget targets in bright light,
  leap at 2–6 blocks and climb when colliding horizontally. Creepers start
  fusing inside 3 blocks, keep the fuse inside 7 while visible, count down for
  30 ticks and explode with power 3, or 6 when `powered`. A skeleton killing
  a creeper can drop disc 13/cat. Charged state persists as vanilla `powered`.
- Natural spawning uses the hostile cap, player/spawn exclusion radius of
  24 blocks, loaded eligible chunks, pack attempts and Beta light thresholds.
  Peaceful removes hostiles; distant/idle despawn uses the Beta thresholds.
- Search uses bounded cardinal A*, one-block ascent and at most three-block
  drops, body clearance and closed-door/liquid restrictions. Wander targets use
  Beta's ten candidates and light/grass weights. Work is limited to two path
  rebuilds per tick and 128 nearby simulated mobs, with a rotating chunk order.
  Paths and targeting/fuse progress are runtime state, as in the original NBT.
- Mobs have fluid drag/current/jump behavior, fire/lava/cactus/suffocation and
  drowning damage, and water extinguishes burning. Health, Air, Fire and motion
  persist through the existing NBT writer.
- **Q** throws one selected item with its metadata/damage, a forward impulse
  and 40-tick pickup delay. Network Q sends the protocol-14 drop action and waits
  for server-authoritative inventory/entities.

These rules are implemented, but full bit-for-bit Java simulation parity is
not established: bounded A* tie ordering/capacity, collision integration,
entity render details, other mob types/biome spawning, spider jockeys, entity
retaliation targets, death animation and sleep ambushes still differ or are absent.
TNT/creeper explosions run locally only in singleplayer; multiplayer remains
server-authoritative. Exact Beta terrain generation, armor, pistons and lightning
remain separate pending tasks.

## Checks

- Windows UCRT64 Release build; **27/27 CTest**, including native and software
  OpenGL 1.1. `-Wall -Wextra -Wshadow -Werror -fsyntax-only` on the new simulation
  and account modules.
- `beta_events_test`: cross-chunk leaf support/decay and missing-chunk deferral;
  fire scheduling/rain/netherrack/lava/TNT/extinction; TNT fuse and chain entity
  retention; blast resistance/wall shielding/player impulse; PrimedTnt and charged
  creeper/Air NBT; Q stack metadata/delay; obstacle path; skeleton arrows, creeper
  fuse and day/night spawn conditions.
- Renderer regression: classic/mirrored/modern limb UV, textured menu buttons
  in all states at 1×/2×, nearest filtering, previews/first person and GL state
  restoration. Existing container/sign/chest/liquid/network regressions pass.
- Real menu/profile/inventory screenshots at 640×480, 960×720 and 1280×720;
  startup from `C:/Windows/Temp` resolves executable-relative assets and both
  existing Beta worlds. User comparison screenshots are ignored, not committed.
- Live HTTPS helper reached the unauthenticated Minecraft profile endpoint
  with expected HTTP 401. Live Microsoft sign-in awaits the account owner's
  browser authorization and service access. The supplied public client
  ID receives a real device code; mock tests also cover the observed longer code.
  These checks do not establish a completed account sign-in.

Actual Snow Leopard/i386/GMA 950 execution remains unverified.
