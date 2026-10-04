"""Exercise the real McRegion reader/writer against the generated all-block save."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from create_block_lab import create_lab

with tempfile.TemporaryDirectory(prefix='recraft-block-lab-') as root:
    target = Path(root) / 'Lab'
    entries = create_lab(target)
    assert set(row[0] for row in entries) == set(range(97))
    before = {p.relative_to(target): hashlib.sha256(p.read_bytes()).digest()
              for p in target.rglob('*') if p.is_file()}
    try:
        create_lab(target)
        raise AssertionError('Generator overwrote an existing save')
    except FileExistsError:
        pass
    after = {p.relative_to(target): hashlib.sha256(p.read_bytes()).digest()
             for p in target.rglob('*') if p.is_file()}
    assert before == after
    subprocess.run([sys.argv[1], str(target)], check=True)
    print('Exclusive save creation and Beta McRegion integration passed')
