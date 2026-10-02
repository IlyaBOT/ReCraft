"""Sync Beta registry columns without overwriting the audited implementation status.

Development only: docs/BETA_COMPATIBILITY_MATRIX.md owns the manually audited
status columns and prose. This tool refreshes IDs/names/metadata usage from the
JAR-derived catalog, validates all 97 rows, and preserves the rest.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "src/world/beta_blocks.def"
OUTPUT = ROOT / "docs/BETA_COMPATIBILITY_MATRIX.md"
ENTRY = re.compile(r'^BETA_BLOCK\(\s*(\d+),\s*(\w+),\s*"([^"]+)",\s*"([^"]+)"\)')
registry = {}
for line in CATALOG.read_text(encoding="utf-8").splitlines():
    match = ENTRY.match(line)
    if match:
        block_id, _, name, usage = match.groups()
        registry[int(block_id)] = (name, usage)
assert sorted(registry) == list(range(97))
lines = OUTPUT.read_text(encoding="utf-8").splitlines()
seen = set()
for index, line in enumerate(lines):
    if not re.match(r"^\| \d+ \|", line):
        continue
    row = [cell.strip() for cell in line.strip("|").split("|")]
    assert len(row) == 15, f"Malformed matrix row {index + 1}"
    block_id = int(row[0])
    assert block_id in registry and block_id not in seen
    row[1], row[2] = registry[block_id]
    lines[index] = "| " + " | ".join(row) + " |"
    seen.add(block_id)
assert seen == set(registry), "Every Beta block needs an audited matrix row"
OUTPUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"Synced {len(seen)} registry rows; audited statuses preserved")
