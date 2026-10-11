#!/usr/bin/env python3
"""Materialize locked upstream sources, never historical application code. Python 3.9+."""
import hashlib
import gzip
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import sys
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / '.build'
CACHE = ROOT / '.source-cache'
HIST = CACHE / 'historical-20261009'

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda: f.read(1024 * 1024), b''): h.update(b)
    return h.hexdigest()

def locked_file(spec, historical):
    name = spec.get('archive_filename', spec.get('filename'))
    expected = spec.get('archive_sha256', spec.get('sha256'))
    size = spec.get('archive_bytes', spec.get('bytes'))
    candidates = [historical / name, CACHE / 'locked' / name]
    p = next((x for x in candidates if x.is_file()), candidates[-1])
    if not p.exists():
        p.parent.mkdir(parents=True, exist_ok=True)
        url = spec.get('project_release_url', spec.get('official_source_url', spec.get('release_url', spec.get('url'))))
        if not url: raise ValueError('No locked source URL: ' + name)
        tmp = p.with_suffix(p.suffix + '.tmp')
        with urllib.request.urlopen(url, timeout=90) as src, tmp.open('wb') as dst: shutil.copyfileobj(src, dst)
        tmp.replace(p)
    stat = p.stat()
    receipt = CACHE / 'receipts' / (name + '.json')
    key = {'sha256': expected, 'bytes': stat.st_size, 'mtime_ns': stat.st_mtime_ns, 'path': str(p)}
    if size and stat.st_size != size: raise ValueError('Source size mismatch: ' + name)
    previous = json.loads(receipt.read_text(encoding="utf-8")) if receipt.exists() else None
    if previous != key:
        if digest(p) != expected: raise ValueError('Source hash mismatch: ' + name)
        receipt.parent.mkdir(parents=True, exist_ok=True)
        receipt.write_text(json.dumps(key, sort_keys=True), encoding="utf-8")
    return p

def unpack_tar(src, dest, strip_prefix=1):
    if (dest / '.admitted.json').exists(): return
    staging = dest.with_name(dest.name + '.extracting')
    if staging.exists(): shutil.rmtree(staging)
    staging.mkdir(parents=True)
    with tarfile.open(src, 'r:gz') as t:
        for m in t.getmembers():
            parts = PurePosixPath(m.name).parts
            if m.name.startswith('/') or '..' in parts: raise ValueError('Unsafe archive path')
            parts = parts[strip_prefix:]
            if not parts: continue
            p = staging.joinpath(*parts)
            if m.isdir(): p.mkdir(parents=True, exist_ok=True)
            elif m.isfile():
                p.parent.mkdir(parents=True, exist_ok=True)
                with t.extractfile(m) as f, p.open('wb') as out: shutil.copyfileobj(f, out)
                os.chmod(p, m.mode & 0o777)
            elif m.issym():
                # Upstream CMake contains local relative links; reject links escaping root.
                if os.path.isabs(m.linkname): raise ValueError('Absolute archive link')
                target = (p.parent / m.linkname).resolve()
                if staging.resolve() not in target.parents: raise ValueError('Unsafe archive link')
                p.parent.mkdir(parents=True, exist_ok=True)
                p.symlink_to(m.linkname)
            elif m.islnk(): raise ValueError('Unsupported hardlink')
    (staging / '.admitted.json').write_text(json.dumps({'source': str(src), 'sha256': digest(src)}), encoding="utf-8")
    if dest.exists(): shutil.rmtree(dest)
    staging.replace(dest)

def copy_verified(src, dst, expected):
    # Outputs are checked when source or output changes; unchanged source data is reused.
    receipt = dst.with_name(dst.name + '.admitted.json')
    ss = src.stat()
    if dst.exists() and receipt.exists():
        ds = dst.stat()
        key = {'hash': expected, 'source_mtime': ss.st_mtime_ns, 'mtime': ds.st_mtime_ns, 'size': ds.st_size}
        if json.loads(receipt.read_text(encoding="utf-8")) == key: return
    if digest(src) != expected: raise ValueError('Resource hash mismatch: ' + str(src))
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    ds = dst.stat()
    receipt.write_text(json.dumps({'hash': expected, 'source_mtime': ss.st_mtime_ns, 'mtime': ds.st_mtime_ns, 'size': ds.st_size}), encoding="utf-8")

def admit_original_ui_art():
    # Generated artwork is immutable admitted input, not a runtime generator.
    # Never silently regenerate altered art during a build or skip recipe locks.
    for filename, prefix in [('original_ui_icons.json', 'assets/generated/ui/icons/'),
                             ('original_ui_materials.json', 'assets/generated/ui/materials/')]:
        manifest = json.loads((ROOT / 'assets/manifests' / filename).read_text(encoding='utf-8'))
        recipe = ROOT / manifest['recipe']
        if digest(recipe) != manifest['recipe_sha256']:
            raise ValueError('Original UI recipe changed without artwork: ' + filename)
        seen = set()
        for item in manifest['outputs']:
            path = item['path']
            if not path.startswith(prefix) or '..' in PurePosixPath(path).parts or path in seen:
                raise ValueError('Invalid original UI artwork path: ' + path)
            seen.add(path)
            if digest(ROOT / path) != item['sha256']:
                raise ValueError('Original UI artwork hash mismatch: ' + path)

def prepare():
    admit_original_ui_art()
    lock = json.loads((ROOT / 'data/native_dependencies.lock.json').read_text(encoding="utf-8"))
    for s in lock['dependencies']:
        p = locked_file(s, HIST / 'native-archives')
        unpack_tar(p, BUILD / 'deps' / s['name'])
        for license in s['licenses']:
            if digest(ROOT / license['project_path']) != license['sha256']: raise ValueError('License mismatch')
    if sys.platform == 'darwin':
        s = next(x for x in lock['build_tools'] if x['name'] == 'cmake' and x['platform'] == 'macos-universal')
        unpack_tar(locked_file(s, HIST / 'native-archives'), BUILD / 'toolchain' / 'cmake')
    runtime = BUILD / 'runtime'
    geo = json.loads((ROOT / 'data/geo/gshhg_sources.lock.json').read_text(encoding="utf-8"))
    # Immutable Earth sources support historical geometry regression only.
    # They never enter or load in the Blank-only production application.
    test_resources = BUILD / 'test-resources'
    if (runtime/'geo').exists() and not (test_resources/'geo').exists():
        test_resources.mkdir(parents=True,exist_ok=True)
        (runtime/'geo').rename(test_resources/'geo')
    print('Historical GSHHG test resources:', [x.get('path', x.get('filename')) for x in geo['members']])
    for m in geo['members']:
        name = m.get('path', m.get('filename', m.get('archive_path', '')))
        name = Path(name).name
        if not name.endswith('.b'): continue
        src = HIST / 'geography-sources' / name
        if not src.exists():
            arc = locked_file(geo['archive'], HIST / 'geography-sources')
            with zipfile.ZipFile(arc) as z:
                member = next(x for x in z.namelist() if Path(x).name == name)
                src.parent.mkdir(parents=True, exist_ok=True)
                with z.open(member) as f, src.open('wb') as dst: shutil.copyfileobj(f, dst)
        copy_verified(src, test_resources / 'geo' / name, m['sha256'])
    shutil.rmtree(runtime/'geo',ignore_errors=True)
    # Historical serif source and notices remain untouched. They are no longer
    # runtime dependencies of the complete retro arcade presentation.
    for old in ['NotoSerifCJKsc-Regular.otf','Cinzel.ttf']:
        (runtime/'fonts'/old).unlink(missing_ok=True)
        (runtime/'fonts'/(old+'.admitted.json')).unlink(missing_ok=True)
    arcade=json.loads((ROOT/'assets/manifests/arcade_fonts.json').read_text(encoding="utf-8"))
    compressed=ROOT/arcade['project_path'];font=runtime/'fonts'/arcade['upstream_member']
    if digest(compressed)!=arcade['compressed_sha256']:raise ValueError('Pixel font compressed source changed')
    for notice in arcade['licenses']:
        if digest(ROOT/notice['project_path'])!=notice['sha256']:raise ValueError('Pixel font license changed')
    if not font.exists() or digest(font)!=arcade['original_sha256']:
        raw=gzip.decompress(compressed.read_bytes())
        if len(raw)!=arcade['original_bytes'] or hashlib.sha256(raw).hexdigest()!=arcade['original_sha256']:raise ValueError('Pixel font original bytes mismatch')
        font.parent.mkdir(parents=True,exist_ok=True);font.write_bytes(raw)
    # Immutable definitions admit full approved contracts/packs, not caches or artwork.
    entry = json.loads((ROOT / 'data/contracts/game_v0_9.json').read_text(encoding="utf-8"))
    required = sorted(set(['data/contracts/game_v0_9.json'] + entry['authoritative_contracts'] + entry['runtime_definition_packs']))
    files = [ROOT / name for name in required]
    if len(files) != 23 or any(not p.is_file() for p in files): raise ValueError('Required v0.9 definition package is incomplete')
    manifest = {'format': 'SonnDefinitions1', 'files': [{'path': p.relative_to(ROOT).as_posix(), 'sha256': digest(p)} for p in files]}
    runtime.mkdir(parents=True, exist_ok=True)
    (runtime / 'definitions.json').write_text(json.dumps(manifest, ensure_ascii=False, sort_keys=True, separators=(',', ':')) + '\n', encoding="utf-8")
    print('Locked sources admitted; definitions:', len(files))

if __name__ == '__main__': prepare()
