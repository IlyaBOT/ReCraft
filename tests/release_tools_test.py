"""Publication gates: reject damaged archives and accidental user data."""
import hashlib
import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('publish_release', 'tools/publish_release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
VERSION = '0.2.0-dev'
COMMIT = 'a' * 40


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)
        for platform in ('windows', 'linux', 'macos'):
            self.archive(platform)
        (self.root / 'windows-defender-scan.txt').write_text('exit_code=0\n')

    def archive(self, platform, extra=None, corrupt=False):
        name = f'ReCraft-{VERSION}-{platform}-x64'
        files = {'VERSION': VERSION.encode(), 'BUILD_INFO.txt': f'version={VERSION}\nsource_commit={COMMIT}\nplatform={platform}-x64\n'.encode()}
        files.update({'assets/' + n: b'fixture' for n in Path('assets/runtime_assets.txt').read_text().splitlines() if n})
        files.update({'windows': {'ReCraft.exe': b'MZfixture'},
                      'linux': {'ReCraft': b'\x7fELFfixture'},
                      'macos': {'ReCraft.app/Contents/MacOS/ReCraft': b'fixture'}}[platform])
        if extra:
            files.update(extra)
        manifest = ''.join(f'{hashlib.sha256(data).hexdigest()}  {path}\n' for path, data in files.items())
        files['SHA256SUMS.txt'] = manifest.encode()
        if corrupt:
            files['VERSION'] = b'damaged'
        path = self.root / (name + ('.zip' if platform == 'windows' else '.tar.gz'))
        if platform == 'windows':
            with zipfile.ZipFile(path, 'w') as archive:
                for relative, data in files.items():
                    archive.writestr(name + '/' + relative, data)
        else:
            with tarfile.open(path, 'w:gz') as archive:
                for relative, data in files.items():
                    entry = tarfile.TarInfo(name + '/' + relative)
                    entry.size = len(data)
                    archive.addfile(entry, io.BytesIO(data))
        path.with_name(path.name + '.sha256').write_text(f'{release.sha256(path)}  {path.name}\n')
        return path

    def test_complete_verified_bundle(self):
        self.assertEqual(len(release.prepare(self.root, VERSION, COMMIT)), 5)
        self.assertIn('windows-x64.zip', (self.root / 'SHA256SUMS.txt').read_text())

    def test_user_data_is_rejected_even_with_valid_checksums(self):
        self.archive('windows', {'config/accounts.json': b'private'})
        with self.assertRaisesRegex(ValueError, 'Unexpected runtime files'):
            release.prepare(self.root, VERSION, COMMIT)

    def test_runtime_corruption_is_rejected(self):
        self.archive('windows', corrupt=True)
        with self.assertRaisesRegex(ValueError, 'Runtime checksum mismatch'):
            release.prepare(self.root, VERSION, COMMIT)

    def test_path_traversal_is_rejected(self):
        self.archive('windows', {'../config/accounts.json': b'private'})
        with self.assertRaisesRegex(ValueError, 'Unsafe archive path'):
            release.prepare(self.root, VERSION, COMMIT)

    def test_wrong_revision_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'source revision mismatch'):
            release.prepare(self.root, VERSION, 'b' * 40)

    def test_failed_scan_is_rejected(self):
        (self.root / 'windows-defender-scan.txt').write_text('exit_code=2\n')
        with self.assertRaisesRegex(ValueError, 'Defender scan receipt'):
            release.prepare(self.root, VERSION, COMMIT)

    def test_archive_corruption_is_rejected(self):
        path = self.root / f'ReCraft-{VERSION}-windows-x64.zip'
        with path.open('ab') as stream:
            stream.write(b'corrupt')
        with self.assertRaisesRegex(ValueError, 'Archive checksum mismatch'):
            release.prepare(self.root, VERSION, COMMIT)
