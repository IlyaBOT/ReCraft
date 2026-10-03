# Minecraft Beta 1.7.3 compatibility matrix

IDs and registry names come from the installed Beta 1.7.3 client JAR's block
initializer (`uu.class`). ID 0 is air; ID 35 is constructed without an explicit
numeric constructor argument and is confirmed by its `ee.class` constructor.
IDs 34 and 36 have no unlocalized name in the initializer; their labels
describe the piston extension and moving piston classes.
`tools/verify_beta_registry.py CLIENT_JAR [JAVAP_PATH]` reproduces the ID
and name comparison without adding a runtime dependency on the JAR.

This is a status audit, not a compatibility claim. Chunks now hold all Beta IDs
0..96 and their separate metadata nibbles without the former network ID
projection. Wool colors, log species and spruce leaves now select their
Beta terrain tiles from metadata. The tile formulas were checked against
`ee.class`, `vg.class` and `bk.class` in that JAR. These are still full-cube visual proxies
where the original block uses another shape or tint. Several simple cubes
now use their Beta terrain tiles; glowing redstone ore and glowstone have
their static light emission. Saplings, grass, dead bushes, flowers, mushrooms
and reeds use cutout crossed planes in the same chunk mesh. Ray selection
and its outline use their audited Beta bounds; body collision stays empty.
Their biome tint, growth and drops remain incomplete.
Half slabs now have 0.5-height meshes, selection and body collision; double
slabs keep a full cube. Four Beta material variants choose their terrain
tiles. Local placement merges a new half slab into the matching one below,
keeping metadata; the default local hotbar has no slab item yet. Generic block
drops/pickup exist in survival; exact per-block drops and world-wide light
propagation are not complete.
Normal and redstone torches now have attachment geometry and support drops.
Redstone adds two-tick inversion/burnout, stepped wire propagation, weak/strong
power, lever/button and directional repeaters. Pistons and the remaining
redstone mechanisms are unfinished. Beds support two halves and local sleep;
rain/snow, ice and day/night have a bounded local implementation.
Workbench, furnace, single/double chest and cactus have their own geometry and
behaviour. Water/lava use scheduled Beta decay and sloping meshes. Portal has
CPU animation, light and ambience, without dimension transport.
The native `.rcg` format is not Minecraft's; version 2 stores Beta IDs, and
version 1 is migrated from the original 12 private IDs. Missing Beta chunks are
not generated. See [the gameplay milestone](GAMEPLAY_PARITY.md) for precise
checks and limits. Unlisted block mechanisms remain unfinished.
Lever attachments/UV, jukebox RecordPlayer/discs, rail topology and three cart
variants now have a local implementation; see [transport audit](BETA_TRANSPORT.md).

| Block ID | Block name | Metadata usage | Implemented | Rendering | Collision | Drops | Interaction | Scheduled tick | Random tick | Tile entity | Redstone | Lighting | Fluid behavior | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | air | none | ID + metadata stored | none | none | no | no | no | no | no | no | none | no | air |
| 1 | stone | none | ID + metadata stored | approximate | box approximation | cobblestone when harvested | generic break/place | no | no | no | no | local approximation | no | visual proxy |
| 2 | grass | none | ID + metadata stored | Beta top/side UV; fixed tint | box approximation | dirt | generic break/place | no | no | no | no | local approximation | no | visual proxy |
| 3 | dirt | none | ID + metadata stored | approximate | box approximation | dirt | generic break/place | no | no | no | no | local approximation | no | visual proxy |
| 4 | stonebrick | none | ID + metadata stored | approximate | box approximation | cobblestone | generic break/place | no | no | no | no | local approximation | no | visual proxy |
| 5 | wood | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 6 | sapling | tree species | ID + metadata + crossed texture | crossed cutout | none | no | selection-box raycast; generic break/place | no | no | no | no | local approximation | no | crossed plant |
| 7 | bedrock | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 8 | water | fluid level and falling | level + scheduled flow | animated sloping liquid | none | no | bucket/source placement | 5 ticks | no | no | no | local approximation | decay 1; water source joining | fluid foundation |
| 9 | water | fluid level and falling | level + still/moving conversion | animated sloping liquid | none | no | bucket/source placement | on neighbour change | no | no | no | local approximation | decay 1; water source joining | fluid foundation |
| 10 | lava | fluid level and falling | level + scheduled flow | animated sloping liquid | none | no | bucket/source placement | 30 ticks | no | no | no | emission 15 | decay 2; no source joining | fluid foundation |
| 11 | lava | fluid level and falling | level + still/moving conversion | animated sloping liquid | none | no | bucket/source placement | on neighbour change | no | no | no | emission 15 | decay 2; no source joining | fluid foundation |
| 12 | sand | none | ID + metadata stored | approximate | box approximation | no | generic break/place | 3 ticks | no | no | no | local approximation | no | falling without entity animation |
| 13 | gravel | none | ID + metadata stored | approximate | box approximation | no | generic break/place | 3 ticks | no | no | no | local approximation | no | falling without entity animation |
| 14 | oreGold | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 15 | oreIron | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 16 | oreCoal | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 17 | log | tree species | ID + metadata + texture variants | metadata texture | box approximation | no | generic break/place | no | no | no | no | local approximation | no | texture variants |
| 18 | leaves | tree species and decay flag bit 8 | ID + metadata + texture variants | Fast exterior / Fancy internal cutouts; optional opaque tiles | full box | harvest rules/shears + sapling 1/20 | generic break/place | no | no | no | no | four-step log support on random ticks | no | unloaded neighbors defer decay |
| 19 | sponge | none | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 20 | glass | none | ID + metadata stored | approximate | box approximation | no | generic break/place | no | no | no | no | local approximation | no | visual proxy |
| 21 | oreLapis | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 22 | blockLapis | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 23 | dispenser | facing | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 24 | sandStone | none | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 25 | musicBlock | none | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 26 | bed | facing, head, occupied | two halves + saved spawn | rotated Beta top, frame/legs cutout, hidden join | 9/16 height | bed from foot | place/sleep/wake | orphan check | no | no | no | day/night | no | partial bed/camera; no nightmares |
| 27 | goldenRail | shape and powered | 0..5 + power bit 8 | sloping cutout rail | none | rail 27 | connectivity/support/cart path | neighbor chain | no | no | eight-rail powered reach | day/night | no | local transport |
| 28 | detectorRail | shape and powered | 0..5 + detection bit 8 | sloping cutout rail | none | rail 28 | cart AABB detection | 20-tick recheck | no | no | weak/strong output | day/night | no | local transport |
| 29 | pistonStickyBase | facing and extended | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 30 | web | none | ID + metadata stored | crossed Beta cutout planes | none | mining harvest rules | generic break/place | no | no | no | no | local approximation | no | slowing/entity behavior unfinished |
| 31 | tallgrass | plant variant | ID + metadata + crossed texture | crossed cutout | none | no | selection-box raycast; generic break/place | no | no | no | no | local approximation | no | crossed plant |
| 32 | deadbush | none | ID + metadata + crossed texture | crossed cutout | none | no | selection-box raycast; generic break/place | no | no | no | no | local approximation | no | crossed plant |
| 33 | pistonBase | facing and extended | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 34 | pistonExtension | facing and sticky | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 35 | cloth | wool color | ID + metadata + texture variants | metadata texture | box approximation | no | generic break/place | no | no | no | no | local approximation | no | texture variants |
| 36 | pistonMoving | moving block state | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 37 | flower | none | ID + metadata + crossed texture | crossed cutout | none | no | selection-box raycast; generic break/place | no | no | no | no | local approximation | no | crossed plant |
| 38 | rose | none | ID + metadata + crossed texture | crossed cutout | none | no | selection-box raycast; generic break/place | no | no | no | no | local approximation | no | crossed plant |
| 39 | mushroom | none | ID + metadata + crossed texture | crossed cutout | none | no | selection-box raycast; generic break/place | no | no | no | no | emission 1; local approximation | no | crossed plant |
| 40 | mushroom | none | ID + metadata + crossed texture | crossed cutout | none | no | selection-box raycast; generic break/place | no | no | no | no | local approximation | no | crossed plant |
| 41 | blockGold | none | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 42 | blockIron | none | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 43 | stoneSlab | slab material | ID + metadata + slab material | double slab cube | full box | no | generic break/place | no | no | no | no | local approximation | no | double slab |
| 44 | stoneSlab | slab material | ID + metadata + slab material | half-height opaque mesh | half-height box | no | selection-box raycast; matching local merge | no | no | no | no | local approximation | no | half slab |
| 45 | brick | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 46 | tnt | none | ID + PrimedTnt Entity/Fuse Byte | Beta top/side/bottom; primed entity | full box | break/drop or prime | fuse/explosion | no | no | redstone primes | fire primes | 80-tick fuse; chain 10-29; blast power 4 | no | blast/particles parity still incomplete |
| 47 | bookshelf | none | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 48 | stoneMoss | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 49 | obsidian | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 50 | torch | attachment face | ID + metadata + attachment mesh | metadata-oriented torch prism | none | torch when broken/unsupported | selection-box raycast; local attachment | support check | no | no | no | emission 14; local approximation | no | normal torch |
| 51 | fire | age 0-15 | ID + metadata + pending ticks | animated CPU cutout planes | non-solid | extinguish; no drop | original fire keys | no | no | no | spread/burn; netherrack/rain/lava | scheduled 40 ticks | no | wall geometry/smoke pending |
| 52 | mobSpawner | none | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 53 | stairsWood | facing | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 54 | chest | facing | 27/54 slots + NBT | Beta single/double chest cube | full cube | chest + contents | open; obstruction/triple checks | no | no | Chest | no | local approximation | no | container |
| 55 | redstoneDust | power level | power metadata | flat cutout | none | redstone dust | dust placement | neighbour updates | no | no | weak/strong, steps | day/night | no | partial redstone |
| 56 | oreDiamond | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 57 | blockDiamond | none | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 58 | workbench | none | 3 x 3 crafting | Beta face textures | full cube | workbench | 151 Beta recipes | no | no | no | no | local approximation | no | crafting |
| 59 | crops | growth stage | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 60 | farmland | moisture | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 61 | furnace | facing | smelting + NBT | oriented Beta faces | full cube | furnace + contents | input/fuel/take-only output | 20 Hz smelting | no | Furnace | no | local approximation | no | container |
| 62 | furnace | facing | smelting + NBT | oriented lit Beta faces | full cube | unlit furnace + contents | input/fuel/take-only output | 20 Hz smelting | no | Furnace | no | emission 13 | no | container |
| 63 | sign | rotation | placement + four saved text lines | textured board/post, bitmap text | none; Beta selection box | item 323 | placement editor | support check | no | Sign; preserves unknown NBT | no | local approximation | replace fluids | local + server-owned text |
| 64 | doorWood | facing, open, upper half | duplicated lower metadata, upper bit 8 | thin Beta atlas model; optional opaque windows | metadata-oriented thin box | door item, support removal | item 324, paired hinge, right-click/power | support validation | no | no | reacts to neighbour power | local approximation | no | doors and reload tested |
| 65 | ladder | attachment face | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 66 | rail | shape | metadata 0..9 connectivity | sloping/corner cutout rail | none | rail 66 | dynamic topology/cart path | neighbor shapes/support | no | no | junction selection | day/night | no | local transport |
| 67 | stairsStone | facing | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 68 | sign | attachment face | placement + four saved text lines | textured wall board, bitmap text | none; Beta selection box | item 323 | placement editor | support check | no | Sign; preserves unknown NBT | no | local approximation | replace fluids | local + server-owned text |
| 69 | lever | attachment and powered | floor 5/6; walls 1..4; bit 8 ON | cobble base + tile 96 prism | none | lever | attach/toggle | support check | no | no | weak/strong support power | day/night | no | cached attachment model |
| 70 | pressurePlate | powered | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 71 | doorIron | facing, open, upper half | duplicated lower metadata, upper bit 8 | thin Beta atlas model; optional opaque windows | metadata-oriented thin box | door item, support removal | item 330, paired hinge, power activation | support validation | no | no | reacts to neighbour power | local approximation | no | power and orientation tested |
| 72 | pressurePlate | powered | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 73 | oreRedstone | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 74 | oreRedstone | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | emission 9; local approximation | no | cube texture |
| 75 | notGate | attachment face | attachment + inversion | unlit narrow prism | none | lit redstone torch | attach/support | 2 ticks | yes (burnout recovery) | no | inversion/burnout | no emission | no | partial redstone |
| 76 | notGate | attachment face | attachment + inversion | lit narrow prism | none | redstone torch | attach/support | 2 ticks | yes | no | inversion/burnout | emission 7 | no | partial redstone |
| 77 | button | attachment and powered | support + held timer | narrow prism | none | button | press, no timer reset | 20 ticks release | no | no | weak/strong support power | day/night | no | partial geometry |
| 78 | snow | none | layer + cold weather placement | 1/8 layer | none | snowball with shovel | break/place | no | weather placement | no | no | day/night | no | melting/support unfinished |
| 79 | ice | none | cube + cold source freezing | transparent cube | full cube | none | break/place | no | weather freezing | no | no | day/night | freezing | melting/break-water unfinished |
| 80 | snow | none | Beta texture | full cube | full cube | 4 snowballs with shovel | break/place | no | no | no | no | day/night | no | cube |
| 81 | cactus | age | age + support + growth | inset cactus faces | inset box | cactus | sand/cactus support; contact damage | support check | growth to 3 blocks | no | no | local approximation | no | cactus |
| 82 | clay | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 83 | reeds | age | ID + metadata + crossed texture | crossed cutout | none | no | selection-box raycast; generic break/place | no | no | no | no | local approximation | no | crossed plant |
| 84 | jukebox | record present | metadata 0/1 + Record ID | Beta tiles 74/75 cube | full box | jukebox + disc | insert/eject 13/cat | no | no | RecordPlayer Record Int | no | day/night | no | local streaming/persistence |
| 85 | fence | none | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 86 | pumpkin | facing | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 87 | hellrock | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 88 | hellsand | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | local approximation | no | cube texture |
| 89 | lightgem | none | ID + metadata + terrain tile | Beta tile (cube) | box approximation | no | generic break/place | no | no | no | no | emission 15; local approximation | no | cube texture |
| 90 | portal | portal axis | portal visual/ambient | animated double-sided plane | none | no | no dimension transport | no | no | no | no | emission 11 | no | visual/ambient only |
| 91 | litpumpkin | facing | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 92 | cake | bites eaten | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 93 | diode | facing and delay | direction/delay + support | 1/8 plate + fixed and movable torches | 1/8 height | repeater item | place/cycle delay | 2/4/6/8 ticks | no | no | directional output | day/night | no | atomic state updates, delay/output tested |
| 94 | diode | facing and delay | direction/delay + support | 1/8 plate + fixed and movable torches | 1/8 height | repeater item | place/cycle delay | 2/4/6/8 ticks | no | no | directional output | day/night | no | atomic state updates, delay/output tested |
| 95 | lockedchest | none | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |
| 96 | trapdoor | facing and open | ID + metadata stored | placeholder/hidden | box approximation | no | generic break/place | no | no | no | no | local approximation | no | storage only |

## Milestone sequence

1. Complete the Beta ID + metadata registry with properties, geometry,
   placement, texture and drop behavior. Raw IDs and nibbles are now preserved.
2. Collision, interaction, neighbor and tick behavior.
3. Light, fluids, falling blocks and tile entities.
4. NBT and McRegion chunk read/write, tested on copies of real Beta saves.
5. JavaRandom and exact Beta terrain generation.
6. World creation/loading, player persistence, then items, containers,
   crafting, mechanisms, entities, survival and protocol work.

The two Beta worlds in `build/saves/` are read-only test fixtures. The
McRegion writer test copies a region file, edits that copy, and verifies it
can read the changed block while retaining nonterrain chunk NBT. Exact Beta terrain generation, most entity AI and many mechanisms remain
future work. Container, Item/mob-entity and supported pending-tick NBT are now saved.
See [the mechanics audit](BETA_MECHANICS_AUDIT.md) for the current task queue.

Player `level.dat` writeback now covers position/rotation/motion, inventory,
Health/Air/Fire, Time/weather, bed spawn and LastPlayed with a one-time backup.
Interactive play acquires and checks the Beta session.lock token. This detects
a competing opener; it is not a transactional OS lock. Fallback loading and
old/new save rotation are implemented.
The Creative catalogue exposes registered IDs/variants; availability there does
not mean that a block's geometry, mechanism or item-use behavior is implemented.
Cross-chunk block light and optional smooth vertex colors are implemented;
full Beta lateral sky-light propagation is still incomplete.
