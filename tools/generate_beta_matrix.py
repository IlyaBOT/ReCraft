"""Regenerate docs/BETA_COMPATIBILITY_MATRIX.md from the Beta ID catalog.

This is a development-only documentation tool; it is not used by the game or
by the Snow Leopard build. The statuses below are audited against current
ReCraft code and must be revised as each subsystem becomes real.
"""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "src/world/beta_blocks.def"
OUTPUT = ROOT / "docs/BETA_COMPATIBILITY_MATRIX.md"
ENTRY = re.compile(r'^BETA_BLOCK\(\s*(\d+),\s*(\w+),\s*"([^"]+)",\s*"([^"]+)"\)')

# These IDs have hand-written approximations in world.c. Other IDs retain
# their real bytes but render as a solid placeholder or remain hidden.
APPROXIMATE = {1, 2, 3, 4, 5, 8, 9, 12, 13, 17, 18, 20, 50}
TEXTURE_VARIANTS = {17, 18, 35}
CUBE_TEXTURES = {5, 7, 14, 15, 16, 21, 22, 45, 48, 49, 56,
                 73, 74, 82, 87, 88, 89}
CROSS_PLANTS = {6, 31, 32, 37, 38, 39, 40, 83}
SLABS = {43, 44}
TORCHES = {50, 75, 76}
APPROXIMATE |= TEXTURE_VARIANTS | CUBE_TEXTURES | CROSS_PLANTS | SLABS

rows = []
for line in CATALOG.read_text(encoding="utf-8").splitlines():
    match = ENTRY.match(line)
    if match:
        block_id, _, name, usage = match.groups()
        block_id = int(block_id)
        approx = block_id in APPROXIMATE
        rows.append([
            str(block_id), name, usage,
            "ID + metadata + attachment mesh" if block_id == 50 else
                "ID + metadata + selection bounds" if block_id in (75, 76) else
                "ID + metadata + slab material" if block_id in SLABS else
                "ID + metadata + crossed texture" if block_id in CROSS_PLANTS else
                "ID + metadata + texture variants" if block_id in TEXTURE_VARIANTS else
                "ID + metadata + terrain tile" if block_id in CUBE_TEXTURES else
                "ID + metadata stored",
            "metadata-oriented torch prism" if block_id == 50 else
                "hidden" if block_id in (75, 76) else
                "half-height opaque mesh" if block_id == 44 else
                "double slab cube" if block_id == 43 else
                "crossed cutout" if block_id in CROSS_PLANTS else
                "metadata texture" if block_id in TEXTURE_VARIANTS else
                "Beta tile (cube)" if block_id in CUBE_TEXTURES else
                "approximate" if approx else "none" if block_id == 0 else "placeholder/hidden",
            "half-height box" if block_id == 44 else
                "full box" if block_id == 43 else
                "none" if block_id in CROSS_PLANTS or block_id in TORCHES or block_id == 0 else "box approximation",
            "no", "selection-box raycast; local attachment" if block_id == 50 else
                "selection-box raycast; redstone pending" if block_id in (75, 76) else
                "selection-box raycast; generic break/place" if block_id in CROSS_PLANTS else
                "selection-box raycast; matching local merge" if block_id == 44 else
                "generic break/place" if block_id == 43 else
                "generic break/place" if block_id else "no",
            "no", "no", "no", "no",
            "emission 14; local approximation" if block_id == 50 else
                "missing" if block_id in (75, 76) else
                "emission 15; local approximation" if block_id == 89 else
                "emission 9; local approximation" if block_id == 74 else
                "emission 1; local approximation" if block_id == 39 else
                "local approximation" if block_id else "none",
            "static water approximation" if block_id in (8, 9) else "no",
            "air" if block_id == 0 else "normal torch partial" if block_id == 50 else
                "redstone torch selection only" if block_id in (75, 76) else
                "half slab" if block_id == 44 else
                "double slab" if block_id == 43 else
                "crossed plant" if block_id in CROSS_PLANTS
                else "texture variants" if block_id in TEXTURE_VARIANTS
                else "cube texture" if block_id in CUBE_TEXTURES
                else "visual proxy" if approx else "storage only",
        ])

assert [int(row[0]) for row in rows] == list(range(97))
header = ["Block ID", "Block name", "Metadata usage", "Implemented", "Rendering",
          "Collision", "Drops", "Interaction", "Scheduled tick", "Random tick",
          "Tile entity", "Redstone", "Lighting", "Fluid behavior", "Status"]
out = [
    "# Minecraft Beta 1.7.3 compatibility matrix",
    "",
    "IDs and registry names come from the installed Beta 1.7.3 client JAR's block",
    "initializer (`uu.class`). ID 0 is air; ID 35 is constructed without an explicit",
    "numeric constructor argument and is confirmed by its `ee.class` constructor.",
    "IDs 34 and 36 have no unlocalized name in the initializer; their labels",
    "describe the piston extension and moving piston classes.",
    "`tools/verify_beta_registry.py CLIENT_JAR [JAVAP_PATH]` reproduces the ID",
    "and name comparison without adding a runtime dependency on the JAR.",
    "",
    "This is a status audit, not a compatibility claim. Chunks now hold all Beta IDs",
    "0..96 and their separate metadata nibbles without the former network ID",
    "projection. Wool colors, log species and spruce leaves now select their",
    "Beta terrain tiles from metadata. The tile formulas were checked against",
    "`ee.class`, `vg.class` and `bk.class` in that JAR. These are still full-cube visual proxies",
    "where the original block uses another shape or tint. Several simple cubes",
    "now use their Beta terrain tiles; glowing redstone ore and glowstone have",
    "their static light emission. Saplings, grass, dead bushes, flowers, mushrooms",
    "and reeds use cutout crossed planes in the same chunk mesh. Ray selection",
    "and its outline use their audited Beta bounds; body collision stays empty.",
    "Their biome tint, growth and drops remain incomplete.",
    "Half slabs now have 0.5-height meshes, selection and body collision; double",
    "slabs keep a full cube. Four Beta material variants choose their terrain",
    "tiles. Local placement merges a new half slab into the matching one below,",
    "keeping metadata; the current local hotbar has no slab item yet. Drops and",
    "world-wide light propagation are not complete.",
    "The normal torch uses attachment metadata for its cached prism, ray",
    "selection and local placement. Redstone torches share the audited",
    "selection bounds but still have no rendered geometry or redstone logic.",
    "Other IDs use a generic solid or hidden proxy, so shape, collision and",
    "lighting can be wrong. The native `.rcg` format is still not a Minecraft",
    "save format: version 2 stores Beta IDs;",
    "version 1 loads through an explicit 12-ID migration. Procedural world generation",
    "is not Beta-compatible. Block-specific metadata behavior and textures are pending.",
    "No tile entities, redstone simulation, scheduled or random block ticks,",
    "drops, or true fluids are present.",
    "",
    "| " + " | ".join(header) + " |",
    "| " + " | ".join("---" for _ in header) + " |",
]
out += ["| " + " | ".join(row) + " |" for row in rows]
out += [
    "",
    "## Milestone sequence",
    "",
    "1. Complete the Beta ID + metadata registry with properties, geometry,",
    "   placement, texture and drop behavior. Raw IDs and nibbles are now preserved.",
    "2. Collision, interaction, neighbor and tick behavior.",
    "3. Light, fluids, falling blocks and tile entities.",
    "4. NBT and McRegion chunk read/write, tested on copies of real Beta saves.",
    "5. JavaRandom and exact Beta terrain generation.",
    "6. World creation/loading, player persistence, then items, containers,",
    "   crafting, mechanisms, entities, survival and protocol work.",
    "",
    "The two Beta worlds in `build/saves/` remain read-only fixtures until a",
    "separate copied-world write test exists.",
    "",
]
OUTPUT.write_text("\n".join(out), encoding="utf-8")
print(f"Wrote {OUTPUT} with {len(rows)} block rows")
