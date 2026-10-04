"""Create a NEW flat Beta 1.7.3 McRegion save. Never overwrite a directory.

Usage: python tools/create_block_lab.py build/saves/ReCraft_Block_Lab_022
Requires only Python's standard library; not a client runtime dependency.
"""
import argparse
import csv
import gzip
import math
from pathlib import Path
import re
import struct
import zlib


def string(value):
    data = value.encode('utf-8')
    return struct.pack('>H', len(data)) + data


def tag(kind, name, value):
    return bytes([kind]) + string(name) + value


def number(kind, name, value):
    return tag(kind, name, struct.pack({1: '>b', 2: '>h', 3: '>i', 4: '>q', 5: '>f', 6: '>d'}[kind], value))


def compound(name, fields):
    return tag(10, name, b''.join(fields) + b'\0')


def array(name, data):
    return tag(7, name, struct.pack('>i', len(data)) + data)


def list_tag(name, kind, values):
    return tag(9, name, bytes([kind]) + struct.pack('>i', len(values)) + b''.join(values))


def variants(block):
    if block in (6, 17, 18, 31):
        return range(3)
    if block in (35, 55, 93, 94):
        return range(16)
    if block in (26, 43, 44, 53, 67, 86, 91):
        return range(4)
    if block in (59, 60, 78, 96):
        return range(8)
    if block in (8, 9, 10, 11):
        return range(16)
    if block in (27, 28):
        return [m | power for power in (0, 8) for m in range(6)]
    if block in (64, 71):
        return range(8)  # Each specimen also includes the matching upper half.
    if block in (23, 61, 62, 65, 68):
        return range(2, 6)
    if block in (50, 75, 76):
        return range(1, 6)
    if block == 69:
        return [m | on for on in (0, 8) for m in range(1, 7)]
    if block == 77:
        return [m | on for on in (0, 8) for m in range(1, 5)]
    if block in (70, 72, 84):
        return range(2)
    if block in (29, 33, 34):
        return [m | on for on in (0, 8) for m in range(6)]
    if block == 66:
        return range(10)
    if block == 63:
        return range(16)
    if block == 92:
        return range(6)
    return [0]


def create_lab(destination):
    destination = Path(destination)
    registry = Path(__file__).resolve().parents[1] / 'src/world/beta_blocks.def'
    names = {int(m[1]): m[2] for m in re.finditer(
        r'BETA_BLOCK\(\s*(\d+),\s*\w+,\s*"([^"]+)"', registry.read_text(encoding='utf-8'))}
    if set(names) != set(range(97)):
        raise ValueError('Expected the complete Beta registry 0..96')
    # Exclusive creation happens before any writes. Existing saves are untouched.
    destination.mkdir(parents=False, exist_ok=False)
    (destination / 'region').mkdir()
    chunks = {}
    entries = []

    def chunk(cx, cz):
        if (cx, cz) not in chunks:
            blocks = bytearray(32768)
            for x in range(16):
                for z in range(16):
                    offset = (x * 16 + z) * 128
                    blocks[offset] = 7
                    blocks[offset + 1:offset + 63] = bytes([3]) * 62
                    blocks[offset + 63] = 2
            chunks[cx, cz] = [blocks, bytearray(16384), []]
        return chunks[cx, cz]

    def put(x, y, z, block, meta=0):
        blocks, data, _ = chunk(x // 16, z // 16)
        index = ((x & 15) * 16 + (z & 15)) * 128 + y
        blocks[index] = block
        shift = (index & 1) * 4
        data[index // 2] = (data[index // 2] & ~(15 << shift)) | ((meta & 15) << shift)

    def tile(x, y, z, kind, extra=()):
        fields = [tag(8, 'id', string(kind))] + [number(3, key, value) for key, value in zip(('x', 'y', 'z'), (x, y, z))]
        chunk(x // 16, z // 16)[2].append(b''.join(fields + list(extra)) + b'\0')

    def sign(x, y, z, lines):
        put(x, y, z, 63, 8)
        tile(x, y, z, 'Sign', [tag(8, 'Text' + str(i + 1), string(text[:15])) for i, text in enumerate(lines)])

    specimens = [(i, list(variants(i))[0]) for i in range(97)]
    specimens += [(i, m) for i in range(97) for m in list(variants(i))[1:]]
    for index, (block, meta) in enumerate(specimens):
        x, y, z = 4 + (index % 12) * 8, 64, 4 + (index // 12) * 8
        # White pedestal and supports keep specimens separate from the grass.
        put(x, y - 1, z, 35, 0)
        if block in (8, 9, 10, 11):
            for a, b in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                put(x + a, y, z + b, 49)
        if block in (6, 31, 37, 38):
            put(x, y - 1, z, 2)
        if block in (32, 81, 83):
            put(x, y - 1, z, 12)
        if block == 59:
            put(x, y - 1, z, 60, 7)
        if block in (50, 75, 76, 69, 77) and (meta & 7) < 5:
            dx, dz = {1: (-1, 0), 2: (1, 0), 3: (0, -1), 4: (0, 1)}[meta & 7]
            put(x + dx, y, z + dz, 1)
        if block in (65, 68):
            dx, dz = {2: (0, 1), 3: (0, -1), 4: (1, 0), 5: (-1, 0)}[meta]
            put(x + dx, y, z + dz, 1)
        if block == 96:
            dx, dz = ((0, 1), (0, -1), (1, 0), (-1, 0))[meta & 3]
            put(x + dx, y, z + dz, 1)
        put(x, y, z, block, meta)
        if block in (64, 71):
            put(x, y + 1, z, block, meta | 8)
        if block == 26:
            dx, dz = ((0, 1), (-1, 0), (0, -1), (1, 0))[meta]
            put(x + dx, y, z + dz, 26, meta | 8)
            put(x + dx, y - 1, z + dz, 35)
        if block in (27, 28, 66):
            shape = meta if block == 66 else meta & 7
            connections = {0: ((0, -1, 0), (0, 1, 0)), 1: ((-1, 0, 0), (1, 0, 0)),
                2: ((-1, 0, 0), (1, 0, 1)), 3: ((1, 0, 0), (-1, 0, 1)),
                4: ((0, 1, 0), (0, -1, 1)), 5: ((0, -1, 0), (0, 1, 1)),
                6: ((1, 0, 0), (0, 1, 0)), 7: ((-1, 0, 0), (0, 1, 0)),
                8: ((-1, 0, 0), (0, -1, 0)), 9: ((1, 0, 0), (0, -1, 0))}[shape]
            for dx, dz, up in connections:
                put(x + dx, y - 1 + up, z + dz, 1)
                put(x + dx, y + up, z + dz, block, (1 if dx else 0) | (meta & 8 if block != 66 else 0))
        if block in (54, 23, 61, 62):
            tile(x, y, z, {54: 'Chest', 23: 'Trap', 61: 'Furnace', 62: 'Furnace'}[block],
                 [list_tag('Items', 10, [])] + ([number(2, 'BurnTime', 0), number(2, 'CookTime', 0)] if block in (61, 62) else []))
        if block in (63, 68):
            tile(x, y, z, 'Sign', [tag(8, 'Text' + str(i), string('Beta sign' if i == 1 else '')) for i in range(1, 5)])
        if block == 84:
            tile(x, y, z, 'RecordPlayer', [number(3, 'Record', 2256 if meta else 0)])
        if block == 25:
            tile(x, y, z, 'Music', [number(1, 'note', 0)])
        if block == 52:
            tile(x, y, z, 'MobSpawner', [tag(8, 'EntityId', string('Pig')), number(2, 'Delay', 200)])
        if block == 36:
            tile(x, y, z, 'Piston', [number(3, 'blockId', 33), number(3, 'blockData', 5),
                 number(3, 'facing', 5), number(5, 'progress', .5), number(1, 'extending', 1)])
        sign(x, y, z + 3, [f'ID {block}:{meta}', names[block][:15], 'Beta 1.7.3', 'Right click'])
        entries.append((block, meta, x, y, z))
    # A connected fence demonstrates actual joins separately from the isolated item.
    for x, z in ((4, -4), (5, -4), (6, -4), (5, -5), (5, -3)):
        put(x, 64, z, 85)
    sign(4, 64, 0, ['BLOCK LAB 0.2.2', '97 legacy IDs', 'States labelled', '/gamemode c'])
    # Flat surrounding terrain covers the full catalogue and its viewing border.
    max_z = max(e[4] for e in entries) + 12
    for cx in range(-2, 8):
        for cz in range(-2, math.ceil(max_z / 16) + 2):
            chunk(cx, cz)
    regions = {}
    for (cx, cz), (blocks, data, tiles) in chunks.items():
        light = bytearray(16384)
        height = bytearray(256)
        for x in range(16):
            for z in range(16):
                h = 128
                while h > 0 and blocks[(x * 16 + z) * 128 + h - 1] == 0:
                    h -= 1
                height[z * 16 + x] = h
                for y in range(h, 128):
                    i = (x * 16 + z) * 128 + y
                    light[i // 2] |= 15 << ((i & 1) * 4)
        payload = compound('', [compound('Level', [number(3, 'xPos', cx), number(3, 'zPos', cz),
            number(4, 'LastUpdate', 0), number(1, 'TerrainPopulated', 1), array('Blocks', blocks),
            array('Data', data), array('SkyLight', light), array('BlockLight', bytes(16384)),
            array('HeightMap', height), list_tag('Entities', 10, []), list_tag('TileEntities', 10, tiles),
            list_tag('TileTicks', 10, [])])])
        packed = zlib.compress(payload)
        record = struct.pack('>I', len(packed) + 1) + b'\2' + packed
        regions.setdefault((cx // 32, cz // 32), []).append(((cx & 31) + (cz & 31) * 32, record))
    for (rx, rz), records in regions.items():
        header, body, sector = bytearray(8192), bytearray(), 2
        for slot, record in sorted(records):
            count = (len(record) + 4095) // 4096
            header[slot * 4:slot * 4 + 4] = struct.pack('>I', (sector << 8) | count)
            body.extend(record + bytes(count * 4096 - len(record)))
            sector += count
        (destination / 'region' / f'r.{rx}.{rz}.mcr').write_bytes(header + body)
    player = compound('Player', [list_tag('Pos', 6, [struct.pack('>d', v) for v in (8.5, 65.62, -8.5)]),
        list_tag('Motion', 6, [struct.pack('>d', 0)] * 3), list_tag('Rotation', 5, [struct.pack('>f', v) for v in (0, 0)]),
        number(2, 'Health', 20), number(2, 'Air', 300), number(2, 'Fire', -20), number(1, 'OnGround', 1),
        number(5, 'FallDistance', 0), list_tag('Inventory', 10, [])])
    level = compound('', [compound('Data', [number(3, 'version', 19132),
        tag(8, 'LevelName', string('ReCraft Block Lab 0.2.2')), number(4, 'RandomSeed', 22),
        number(4, 'Time', 6000), number(4, 'LastPlayed', 0), number(4, 'SizeOnDisk', 0),
        number(3, 'SpawnX', 8), number(3, 'SpawnY', 64), number(3, 'SpawnZ', -8),
        number(1, 'raining', 0), number(1, 'thundering', 0), number(3, 'rainTime', 1000000),
        number(3, 'thunderTime', 1000000), player])])
    with (destination / 'level.dat').open('xb') as output:
        output.write(gzip.compress(level, mtime=0))
    with (destination / 'block-lab.csv').open('x', newline='', encoding='utf-8') as output:
        writer = csv.writer(output)
        writer.writerow(('id', 'metadata', 'x', 'y', 'z'))
        writer.writerows(entries)
    (destination / 'README.txt').write_text(
        'Separate Beta 1.7.3 McRegion fixture. All 97 IDs and audited metadata states.\n'
        'Technical states (air, moving piston, powered components) are initial snapshots; simulation updates them.\n'
        'Use /gamemode creative in ReCraft. No modern block IDs or GameType field.\n'
        'The generator refuses to overwrite this directory. Spawner AI and the obsolete locked-chest store are not implemented.\n', encoding='utf-8')
    return entries


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    print(f'Created {len(create_lab(args.destination))} specimens in {args.destination}')
