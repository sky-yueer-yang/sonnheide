#!/usr/bin/env python3
"""Admit immutable full GSHHG geometry, Python 3.9+, standard library.

No DEM data, no network in runtime. --fetch obtains the project Release; the
explicit --allow-upstream option rebuilds a missing Release from locked official
bytes. All five LODs are unpacked and SHA verified; only full is authoritative.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import time
import urllib.error
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / "data/geo/gshhg_sources.lock.json"
CACHE = ROOT / ".build/geography-sources"
USER_AGENT = "Sonnheide-GeographyArchive/1.0 (https://github.com/sky-yueer-yang/sonnheide)"


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def verify(path, item):
    if (not path.is_file() or path.is_symlink() or path.stat().st_size != item["bytes"]
            or digest(path) != item["sha256"]):
        raise ValueError("Geography source size/SHA mismatch: " + str(path))


def validate_lock():
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    if (lock["schema_version"] != 1 or lock["dataset"] != "GSHHG"
            or lock["version"] != "2.3.7" or lock["license"] != "LGPL-3.0-or-later"
            or {m["filename"] for m in lock["members"]} != {"gshhs_" + d + ".b" for d in "fhilc"}
            or lock["archive"]["url"] != "https://www.soest.hawaii.edu/pwessel/gshhg/gshhg-bin-2.3.7.zip"
            or lock["archive"]["official_mirror_url"] != "https://github.com/GenericMappingTools/gshhg-gmt/releases/download/2.3.7/gshhg-bin-2.3.7.zip"):
        raise ValueError("Invalid complete geography source lock")
    for item in lock["notices"]:
        path = (ROOT / item["path"]).resolve()
        if ROOT not in path.parents:
            raise ValueError("External notice path")
        verify(path, item)
    return lock


def download(url, path, item):
    partial = path.with_suffix(path.suffix + ".partial")
    for attempt in range(3):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(request, timeout=60) as source, partial.open("wb") as target:
                shutil.copyfileobj(source, target, length=1024 * 1024)
            verify(partial, item)
            partial.replace(path)
            return
        except (urllib.error.URLError, TimeoutError, ConnectionError) as error:
            # Missing immutable Releases fail immediately; transient source/network
            # failures get bounded retries. A size/SHA mismatch is never retried.
            permanent = isinstance(error, urllib.error.HTTPError) and error.code not in (408, 429, 500, 502, 503, 504)
            if permanent or attempt == 2:
                raise
            time.sleep(2 ** (attempt + 1))
        finally:
            if partial.exists():
                partial.unlink()


def prepare(fetch=False, allow_upstream=False, destination=CACHE):
    lock = validate_lock()
    if not fetch:
        return lock, None
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    source = destination / lock["archive"]["filename"]
    if source.exists():
        verify(source, lock["archive"])
    else:
        try:
            download(lock["archive"]["release_url"], source, lock["archive"])
        except (urllib.error.URLError, urllib.error.HTTPError):
            if not allow_upstream:
                raise
            try:
                download(lock["archive"]["official_mirror_url"], source, lock["archive"])
            except (urllib.error.URLError, TimeoutError, ConnectionError):
                download(lock["archive"]["url"], source, lock["archive"])
    with zipfile.ZipFile(source) as archive:
        for item in lock["members"]:
            name = item["filename"]
            info = archive.getinfo(name)
            if info.file_size != item["bytes"]:
                raise ValueError("Invalid original geography member")
            path = destination / name
            if not path.exists():
                partial = path.with_suffix(".partial")
                with archive.open(name) as upstream, partial.open("wb") as target:
                    shutil.copyfileobj(upstream, target, length=1024 * 1024)
                verify(partial, item)
                partial.replace(path)
            verify(path, item)
    # Receipts assist diagnostics; C++ still independently SHA verifies each file.
    (destination / "ADMITTED.json").write_text(json.dumps(lock, indent=2) + "\n", encoding="utf-8")
    notices = destination / "notices"
    notices.mkdir(exist_ok=True)
    for item in lock["notices"]:
        shutil.copyfile(ROOT / item["path"], notices / Path(item["path"]).name)
    return lock, source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fetch", action="store_true")
    parser.add_argument("--allow-upstream", action="store_true")
    parser.add_argument("--destination", type=Path, default=CACHE)
    args = parser.parse_args()
    lock, source = prepare(args.fetch, args.allow_upstream, args.destination)
    print("GSHHG " + lock["version"] + ": " + ("full five-LOD bytes admitted at " + str(source.parent) if source else "source lock/notices verified"))


if __name__ == "__main__":
    main()
