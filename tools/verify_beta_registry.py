"""Compare the development block catalog with a local Beta 1.7.3 client JAR.

Usage: python tools/verify_beta_registry.py CLIENT_JAR [JAVAP_PATH]
Only reads the supplied JAR. Requires a JDK; never used by the game build.
"""

from pathlib import Path
import re
import subprocess
import sys

if len(sys.argv) not in (2, 3):
    raise SystemExit(__doc__)
jar = Path(sys.argv[1])
javap = sys.argv[2] if len(sys.argv) == 3 else "javap"
if not jar.is_file():
    raise SystemExit(f"Not a file: {jar}")

def disassemble(class_name):
    return subprocess.run(
        [javap, "-c", "-private", "-classpath", str(jar), class_name],
        check=True, capture_output=True, text=True, encoding="utf-8"
    ).stdout

source = disassemble("uu").split("  static {};", 1)[1].splitlines()
actual = {}
for index, line in enumerate(source):
    if not re.search(r"\bnew\s+#\d+\s+// class \w+", line):
        continue
    if index + 2 >= len(source) or "dup" not in source[index + 1]:
        continue
    number = re.search(r"\b(?:iconst_(\d+)|bipush\s+(\d+)|sipush\s+(\d+))\b",
                       source[index + 2])
    if not number:
        continue
    block_id = int(next(group for group in number.groups() if group is not None))
    if block_id > 255:
        continue
    end = index + 2
    while end < min(len(source), index + 100) and "putstatic" not in source[end]:
        end += 1
    if end >= min(len(source), index + 100):
        continue
    if not re.search(r"// Field \w+:L\w+;", source[end]):
        continue
    names = re.findall(r"// String ([^\r\n]+)", "\n".join(source[index:end + 1]))
    actual[block_id] = names[-1] if names else None

# Wool constructs its ID inside ee(), unlike the other block initializer rows.
wool = disassemble("ee")
if not re.search(r"\b0: aload_0\s+1: bipush\s+35\b", wool):
    raise SystemExit("The supplied JAR does not contain the expected wool constructor")
actual[35] = "cloth"
actual[0] = "air"  # Empty block slot; no Block object is constructed for air.

catalog = Path(__file__).resolve().parents[1] / "src/world/beta_blocks.def"
expected = {}
for line in catalog.read_text(encoding="utf-8").splitlines():
    match = re.match(r'^BETA_BLOCK\(\s*(\d+),\s*\w+,\s*"([^"]+)"', line)
    if match:
        expected[int(match.group(1))] = match.group(2)

errors = []
if set(actual) != set(expected):
    errors.append(f"ID sets differ: JAR={sorted(actual)}, catalog={sorted(expected)}")
for block_id in sorted(set(actual) & set(expected)):
    # The piston helper blocks have no unlocalized name in the initializer.
    if actual[block_id] is not None and actual[block_id] != expected[block_id]:
        errors.append(f"ID {block_id}: JAR={actual[block_id]!r}, catalog={expected[block_id]!r}")
if errors:
    raise SystemExit("\n".join(errors))
print(f"Verified {len(expected)} Beta block IDs against {jar}")
