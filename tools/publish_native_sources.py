#!/usr/bin/env python3
"""Publish only SHA-256 verified, locked original source bytes to this project's GitHub Release.

Requires GitHub CLI and an authenticated Actions token; never overwrites an existing asset.
Python 3.9+ standard library. This is a publication recipe, not part of the application.
"""
import json
from pathlib import Path
import subprocess
import tempfile
import vendor_native

ROOT = Path(__file__).resolve().parents[1]
REPO = 'sky-yueer-yang/sonnheide'
TAG = 'native-sources-v1'


def gh(*arguments, **kwargs):
    return subprocess.run(['gh', *arguments], check=True, **kwargs)


def main():
    lock = json.loads((ROOT / 'data/native_dependencies.lock.json').read_text())
    fonts = json.loads((ROOT / 'assets/manifests/native_fonts.json').read_text())
    entries = lock['dependencies'] + fonts['fonts']
    files = [(entry, vendor_native.archive_for(entry, allow_upstream=True)) for entry in entries]
    import os
    revision = os.environ['GITHUB_SHA']
    existing = subprocess.run(['gh', 'release', 'view', TAG, '--repo', REPO], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if existing.returncode:
        gh('release', 'create', TAG, '--repo', REPO, '--target', revision,
           '--title', 'Locked native client sources v1',
           '--notes', 'Original, unmodified source archives and Noto Serif CJK SC font. Exact versions, SHA-256 hashes, provenance and license notices are retained in data/native_dependencies.lock.json, assets/manifests/native_fonts.json and third_party/native. This source package is not a game release.')
    listing = gh('release', 'view', TAG, '--repo', REPO, '--json', 'assets', capture_output=True, text=True)
    names = {asset['name'] for asset in json.loads(listing.stdout)['assets']}
    for entry, path in files:
        name = entry['archive_filename']
        if name in names:
            with tempfile.TemporaryDirectory() as folder:
                gh('release', 'download', TAG, '--repo', REPO, '--pattern', name, '--dir', folder)
                old = Path(folder) / name
                if old.stat().st_size != entry['archive_bytes'] or vendor_native.digest(old) != entry['archive_sha256']:
                    raise RuntimeError('Existing release asset differs; refusing overwrite: ' + name)
        else:
            gh('release', 'upload', TAG, str(path), '--repo', REPO)
    print('Verified original source assets hosted on project Release:', len(files))


if __name__ == '__main__':
    main()
