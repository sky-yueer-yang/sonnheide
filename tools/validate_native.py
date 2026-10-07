#!/usr/bin/env python3
"""Validate native source locks, redistributed notices and optionally materialized original bytes."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--materialized', action='store_true', help='Require every original archive and native font locally')
    args = parser.parse_args()
    lock = json.loads((ROOT / 'data/native_dependencies.lock.json').read_text())
    fonts = json.loads((ROOT / 'assets/manifests/native_fonts.json').read_text())
    entries = lock['dependencies'] + fonts['fonts']
    require(len({item['name'] for item in entries}) == len(entries), 'Duplicate source identity')
    for item in entries:
        revision = item['revision']
        require(len(revision) == 40 and all(c in '0123456789abcdef' for c in revision), 'Mutable source revision')
        require(revision in item['official_source_url'], 'Official URL does not bind source revision')
        require(item['project_release_url'] == 'https://github.com/sky-yueer-yang/sonnheide/releases/download/native-sources-v1/' + item['archive_filename'], 'Source must be hosted by this project')
        for notice in item['licenses']:
            path = ROOT / notice['project_path']
            require(path.is_file() and digest(path) == notice['sha256'], 'Changed/missing upstream notice: ' + str(path))
        path = ROOT / '.build/native-archives' / item['archive_filename']
        if args.materialized or path.exists():
            require(path.is_file() and path.stat().st_size == item['archive_bytes'] and digest(path) == item['archive_sha256'], 'Invalid source bytes: ' + str(path))
    for item in fonts['fonts']:
        path = ROOT / item['project_path']
        if args.materialized or path.exists():
            require(path.is_file() and path.stat().st_size == item['archive_bytes'] and digest(path) == item['archive_sha256'], 'Invalid packaged font: ' + str(path))
    print('PASS: immutable native source locks, original license notices and available archive/font hashes')


if __name__ == '__main__':
    main()
