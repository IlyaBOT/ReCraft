#!/usr/bin/env python3
"""Validate CI archives, then upload a draft and publish a complete GitHub release.

Uses only Python's standard library. GITHUB_TOKEN is needed only with --publish.
Never replaces files in an already published release.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import tarfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def validate_archive(path, version, platform):
    prefix = f'ReCraft-{version}-{platform}-x64/'
    with zipfile.ZipFile(path) if platform == 'windows' else tarfile.open(path) as archive:
        entries = archive.infolist() if platform == 'windows' else archive.getmembers()
        files = {}
        for entry in entries:
            name = entry.filename if platform == 'windows' else entry.name
            parts = PurePosixPath(name).parts
            is_directory = entry.is_dir() if platform == 'windows' else entry.isdir()
            if name.rstrip('/') == prefix.rstrip('/') and is_directory:
                continue
            if not name.startswith(prefix) or '..' in parts or '\\' in name:
                raise ValueError(f'Unsafe archive path: {name}')
            if platform != 'windows' and not (entry.isdir() or entry.isfile()):
                raise ValueError(f'Unexpected archive entry type: {name}')
            if platform == 'windows' and (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError(f'Unexpected archive symlink: {name}')
            if is_directory:
                continue
            relative = name[len(prefix):]
            if relative in files:
                raise ValueError(f'Duplicate archive entry: {relative}')
            stream = archive.open(entry) if platform == 'windows' else archive.extractfile(entry)
            with stream:
                files[relative] = stream.read()
        manifest = files.get('SHA256SUMS.txt', b'').decode('ascii')
        listed = {}
        for line in manifest.splitlines():
            match = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
            if not match or match[2] in listed:
                raise ValueError('Invalid or duplicate runtime checksum entry')
            listed[match[2]] = match[1]
        if set(listed) != set(files) - {'SHA256SUMS.txt'}:
            raise ValueError('Runtime checksum inventory differs from archive contents')
        for name, digest in listed.items():
            if hashlib.sha256(files[name]).hexdigest() != digest:
                raise ValueError(f'Runtime checksum mismatch: {name}')
        if files.get('VERSION', b'').decode().strip() != version:
            raise ValueError('Archive VERSION differs from release tag')
        assets = {n for n in Path('assets/runtime_assets.txt').read_text().splitlines() if n}
        if {n[7:] for n in files if n.startswith('assets/')} != assets:
            raise ValueError('Archive assets differ from runtime allowlist')
        allowed = {'VERSION', 'MICROSOFT_CLIENT_ID', 'README.md', 'BUILD_INFO.txt', 'SHA256SUMS.txt',
                   'docs/MICROSOFT_ACCOUNT.md', 'docs/BETA_EVENTS_AND_PROFILE.md', 'docs/BETA_BLOCKS_022.md',
                   'licenses/ASSET_SOURCES.md', 'licenses/raylib.md', 'licenses/glfw.txt'}
        allowed |= {'assets/' + name for name in assets}
        if platform == 'windows':
            allowed.add('ReCraft.exe')
            allowed |= {n for n in files if re.fullmatch(r'[A-Za-z0-9_.+-]+\.dll', n)}
            if not files.get('ReCraft.exe', b'').startswith(b'MZ'):
                raise ValueError('Missing Windows executable')
        elif platform == 'linux':
            allowed.add('ReCraft')
            if not files.get('ReCraft', b'').startswith(b'\x7fELF'):
                raise ValueError('Missing Linux executable')
        else:
            allowed |= {'ReCraft.app/Contents/Info.plist', 'ReCraft.app/Contents/MacOS/ReCraft'}
            if not files.get('ReCraft.app/Contents/MacOS/ReCraft'):
                raise ValueError('Missing macOS executable')
        if set(files) - allowed:
            raise ValueError(f'Unexpected runtime files: {sorted(set(files) - allowed)}')
        return dict(line.split('=', 1) for line in files['BUILD_INFO.txt'].decode().splitlines())


def prepare(directory, version, commit):
    archives = []
    for platform in ('windows', 'linux', 'macos'):
        extension = 'zip' if platform == 'windows' else 'tar.gz'
        path = directory / f'ReCraft-{version}-{platform}-x64.{extension}'
        checksum = path.with_name(path.name + '.sha256').read_text().strip()
        if checksum != f'{sha256(path)}  {path.name}':
            raise ValueError(f'Archive checksum mismatch: {path.name}')
        info = validate_archive(path, version, platform)
        if info.get('source_commit') != commit or info.get('version') != version or info.get('platform') != platform + '-x64':
            raise ValueError(f'Archive source revision mismatch: {path.name}')
        archives.append(path)
    report = directory / 'windows-defender-scan.txt'
    if not re.search(r'^exit_code=0$', report.read_text(encoding='utf-8-sig'), re.MULTILINE):
        raise ValueError('Successful Windows Defender scan receipt is required')
    assets = archives + [report]
    sums = directory / 'SHA256SUMS.txt'
    sums.write_text(''.join(f'{sha256(path)}  {path.name}\n' for path in assets), encoding='ascii')
    return assets + [sums]


class GitHub:
    def __init__(self, repository, token):
        if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository):
            raise ValueError('Invalid repository name')
        self.repository = repository
        self.token = token

    def request(self, path, method='GET', data=None, content_type='application/json'):
        url = path if path.startswith('https://uploads.github.com/') else 'https://api.github.com' + path
        if urllib.parse.urlparse(url).hostname not in ('api.github.com', 'uploads.github.com'):
            raise ValueError('Unexpected GitHub API host')
        headers = {'Authorization': 'Bearer ' + self.token, 'Accept': 'application/vnd.github+json',
                   'X-GitHub-Api-Version': '2022-11-28', 'User-Agent': 'ReCraft-release',
                   'Content-Type': content_type}
        if isinstance(data, dict):
            data = json.dumps(data).encode()
        with urllib.request.urlopen(urllib.request.Request(url, data=data, headers=headers, method=method), timeout=120) as response:
            return json.load(response)

    def publish(self, version, commit, assets, notes):
        base = '/repos/' + self.repository
        tag = 'v' + version
        ref = self.request(base + '/git/ref/tags/' + tag)['object']
        if ref['type'] == 'tag':
            ref = self.request(base + '/git/tags/' + ref['sha'])['object']
        if ref['type'] != 'commit' or ref['sha'] != commit:
            raise ValueError('Release tag does not point to the verified CI revision')
        try:
            release = self.request(base + '/releases/tags/' + tag)
        except urllib.error.HTTPError as error:
            if error.code != 404:
                raise
            release = None
        if release and not release['draft']:
            raise ValueError('Release is already published; refusing to replace published binaries')
        body = {'tag_name': tag, 'target_commitish': commit, 'name': 'ReCraft ' + version,
                'body': notes, 'draft': True, 'prerelease': '-' in version}
        if release:
            release = self.request(base + '/releases/' + str(release['id']), 'PATCH', body)
        else:
            release = self.request(base + '/releases', 'POST', body)
        existing = {asset['name']: asset for asset in release['assets']}
        upload_url = release['upload_url'].split('{', 1)[0]
        for path in assets:
            digest = 'sha256:' + sha256(path)
            previous = existing.get(path.name)
            if previous:
                if previous.get('digest') != digest:
                    raise ValueError(f'Draft already has a different asset: {path.name}')
                continue
            content_type = 'application/zip' if path.suffix == '.zip' else 'application/gzip' if path.suffix == '.gz' else 'text/plain'
            uploaded = self.request(upload_url + '?name=' + urllib.parse.quote(path.name), 'POST', path.read_bytes(), content_type)
            if uploaded['size'] != path.stat().st_size or uploaded.get('digest') != digest:
                raise ValueError(f'Uploaded asset verification failed: {path.name}')
        uploaded = self.request(base + '/releases/' + str(release['id']))['assets']
        expected = {p.name: 'sha256:' + sha256(p) for p in assets}
        if {a['name']: a.get('digest') for a in uploaded} != expected:
            raise ValueError('Release asset inventory differs from verified files')
        release = self.request(base + '/releases/' + str(release['id']), 'PATCH', {'draft': False})
        print('Published ' + release['html_url'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--repository', default='IlyaBOT/ReCraft')
    parser.add_argument('--notes', type=Path, default=Path('docs/RELEASE_NOTES.md'))
    parser.add_argument('--publish', action='store_true')
    args = parser.parse_args()
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.-]+)?', args.version) or not re.fullmatch(r'[0-9a-f]{40}', args.commit):
        parser.error('Expected a release version and full Git commit SHA')
    assets = prepare(args.directory, args.version, args.commit)
    print('Verified all three runtime archives, source revision and checksums.')
    if args.publish:
        GitHub(args.repository, os.environ['GITHUB_TOKEN']).publish(args.version, args.commit, assets, args.notes.read_text(encoding='utf-8'))


if __name__ == '__main__':
    main()
