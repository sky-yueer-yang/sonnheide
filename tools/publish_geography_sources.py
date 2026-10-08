#!/usr/bin/env python3
"""Actions-only immutable original geography Release publication and download QA."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import prepare_geography as sources
REPOSITORY = "sky-yueer-yang/sonnheide"
TAG = "geography-sources-v1"


def gh(*args, **kwargs):
    return subprocess.run(["gh", *args], check=True, **kwargs)


def main():
    lock, path = sources.prepare(fetch=True, allow_upstream=True)
    exists = subprocess.run(["gh", "release", "view", TAG, "--repo", REPOSITORY],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if exists.returncode:
        gh("release", "create", TAG, "--repo", REPOSITORY, "--target", os.environ["GITHUB_SHA"],
           "--title", "Locked original global coastlines v1", "--notes",
           "Unmodified official GSHHG 2.3.7 binary archive: full/high/intermediate/low/coarse shorelines "
           "and original documentation under LGPL-3.0-or-later. No elevations. Source lock and notices "
           "are in data/geo/gshhg_sources.lock.json. Maximum source geometry is used by frozen terrain; "
           "older source geography does not guarantee contemporary metre-level accuracy.")
    listing = gh("release", "view", TAG, "--repo", REPOSITORY, "--json", "assets", capture_output=True, text=True)
    names = {a["name"] for a in json.loads(listing.stdout)["assets"]}
    # Keep the original archive byte-for-byte; distribute the GPL text missing
    # from upstream's ZIP as a companion, together with the original notices.
    uploads = [(path, lock["archive"])] + [(sources.ROOT / item["path"], item)
                                            for item in lock["notices"]]
    for source, expected in uploads:
        if source.name not in names:
            gh("release", "upload", TAG, str(source), "--repo", REPOSITORY)
        with tempfile.TemporaryDirectory() as temporary:
            gh("release", "download", TAG, "--repo", REPOSITORY,
               "--pattern", source.name, "--dir", temporary)
            sources.verify(Path(temporary) / source.name, expected)
    print("Hosted original global coastline archive verified by complete SHA-256")


if __name__ == "__main__":
    main()
