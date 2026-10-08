#!/usr/bin/env python3
"""Publish SHA-verified source packs on this project's Release; never overwrite.

Actions-only publication recipe; requires authenticated gh. No game/runtime assets
are synthesized, no credentials are read from disk, no user saves are uploaded.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import prepare_ground_sources as sources

REPOSITORY = "sky-yueer-yang/sonnheide"
TAG = "ground-sky-sources-v1"


def gh(*arguments, **kwargs):
    return subprocess.run(["gh", *arguments], check=True, **kwargs)


def main():
    lock, paths = sources.prepare(fetch=True, allow_upstream=True)
    revision = os.environ["GITHUB_SHA"]
    existing = subprocess.run(["gh", "release", "view", TAG, "--repo", REPOSITORY],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if existing.returncode:
        gh("release", "create", TAG, "--repo", REPOSITORY, "--target", revision,
           "--title", "Locked ground and Pure Sky sources v1", "--notes",
           "Original Poly Haven CC0 sources: eight 2K PNG PBR channel sets and two 8K Pure Sky HDRIs. "
           "Each immutable pack includes the original bytes, author/source/hash receipt and CC0 legal text. "
           "Exact source and archive hashes are in assets/manifests/ground_sky_sources.json. "
           "These are source materials; native PBR/Hex-Tiling/sky and production world creation remain unimplemented.")
    listing = gh("release", "view", TAG, "--repo", REPOSITORY, "--json", "assets", capture_output=True, text=True)
    names = {asset["name"] for asset in json.loads(listing.stdout)["assets"]}
    for entry, path in zip(lock["assets"], paths):
        archive = entry["archive"]
        if archive["filename"] not in names:
            gh("release", "upload", TAG, str(path), "--repo", REPOSITORY)
        # Verify the hosted bytes even after a first upload; command success alone
        # does not establish that the public source pack matches our lock.
        with tempfile.TemporaryDirectory() as folder:
            gh("release", "download", TAG, "--repo", REPOSITORY, "--pattern", archive["filename"], "--dir", folder)
            sources.verify_archive(entry, Path(folder) / archive["filename"],
                                   (sources.ROOT / lock["license_text"]["path"]).read_bytes())
    print("Verified immutable source packs hosted on project GitHub:", len(paths))


if __name__ == "__main__":
    main()
