#!/usr/bin/env python3
"""Fetch immutable, license-reviewed native sources into ignored cache. Python 3.9+ stdlib only."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def archive_for(entry, allow_upstream=False, offline=False):
    cache = ROOT / '.build/native-archives'
    cache.mkdir(parents=True, exist_ok=True)
    path = cache / entry['archive_filename']
    if path.exists() and digest(path) == entry['archive_sha256']:
        return path
    if offline:
        raise RuntimeError('Missing verified offline archive: ' + str(path))
    urls = [entry.get('project_release_url', entry['official_source_url'])]
    if allow_upstream and entry['official_source_url'] not in urls:
        urls.append(entry['official_source_url'])
    for url in urls:
        temp = path.with_suffix('.download')
        try:
            print('Downloading ' + url, flush=True)
            with urllib.request.urlopen(url, timeout=60) as source, temp.open('wb') as output:
                shutil.copyfileobj(source, output)
            if digest(temp) != entry['archive_sha256'] or temp.stat().st_size != entry['archive_bytes']:
                raise RuntimeError('SHA-256/size mismatch: ' + url)
            temp.replace(path)
            return path
        except Exception as error:
            temp.unlink(missing_ok=True)
            if url == urls[-1]:
                raise RuntimeError('Cannot obtain verified archive; --allow-upstream permits official bootstrap: ' + str(error)) from error
    raise RuntimeError('No archive source')


def extract_source(entry, archive):
    base = ROOT / '.build/native-sources'
    base.mkdir(parents=True, exist_ok=True)
    destination = base / entry['name']
    marker = destination / '.sonnheide-archive-sha256'
    if marker.exists() and marker.read_text().strip() == entry['archive_sha256']:
        return
    temp = base / (entry['name'] + '.extract')
    if temp.exists():
        shutil.rmtree(temp)
    temp.mkdir()
    with tarfile.open(archive, 'r:gz') as tar:
        members = tar.getmembers()
        tops = set()
        for member in members:
            item = Path(member.name)
            if item.is_absolute() or '..' in item.parts or member.isdev() or member.issym() or member.islnk():
                raise RuntimeError('Unsafe source archive member: ' + member.name)
            tops.add(item.parts[0])
        if len(tops) != 1:
            raise RuntimeError('Archive must have one source root')
        tar.extractall(temp)
    if destination.exists():
        shutil.rmtree(destination)
    (temp / next(iter(tops))).replace(destination)
    temp.rmdir()
    marker.write_text(entry['archive_sha256'] + '\n')
    print('Verified source: ' + entry['name'], flush=True)


def restore_fonts(allow_upstream=False, offline=False, archives_only=False):
    manifest = ROOT / 'assets/manifests/native_fonts.json'
    if not manifest.exists():
        return
    for entry in json.loads(manifest.read_text())['fonts']:
        source = ROOT / entry['project_path']
        archive = ROOT / '.build/native-archives' / entry['archive_filename']
        if source.exists() and digest(source) == entry['archive_sha256']:
            archive.parent.mkdir(parents=True, exist_ok=True)
            if not archive.exists() or digest(archive) != entry['archive_sha256']:
                shutil.copyfile(source, archive)
        archive = archive_for(entry, allow_upstream, offline)
        if not archives_only and (not source.exists() or digest(source) != entry['archive_sha256']):
            source.parent.mkdir(parents=True, exist_ok=True)
            temp = source.with_suffix('.download')
            shutil.copyfile(archive, temp)
            temp.replace(source)
        print('Verified font: ' + entry['name'], flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--allow-upstream', action='store_true')
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--archives-only', action='store_true', help='Fetch/verify archives for Release publication, without extraction')
    args = parser.parse_args()
    lock = json.loads((ROOT / 'data/native_dependencies.lock.json').read_text())
    for entry in lock['dependencies']:
        archive = archive_for(entry, args.allow_upstream, args.offline)
        if not args.archives_only:
            extract_source(entry, archive)
    restore_fonts(args.allow_upstream, args.offline, args.archives_only)


if __name__ == '__main__':
    main()
