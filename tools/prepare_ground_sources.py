#!/usr/bin/env python3
"""Verify/fetch immutable Poly Haven source packs; Python 3.9+, standard library.

No image conversion or GPU integration. Default operation validates the checked-in
source lock/notices without network access. --fetch obtains project Release packs;
--allow-upstream permits rebuilding missing packs from individually SHA-locked originals.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "assets/manifests/ground_sky_sources.json"
CACHE = ROOT / ".build/ground-sky-sources"
USER_AGENT = "Sonnheide-SourceArchive/1.0 (https://github.com/sky-yueer-yang/sonnheide)"
RELEASE_BASE = "https://github.com/sky-yueer-yang/sonnheide/releases/download/ground-sky-sources-v1/"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    hasher = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            hasher.update(block)
    return hasher.hexdigest()


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")


def safe_name(value):
    require(isinstance(value, str) and re.fullmatch(r"[A-Za-z0-9_.-]+", value)
            and value not in (".", ".."), "Unsafe source filename")
    return value


def verify_file(path, expected):
    require(path.is_file() and not path.is_symlink(), "Missing/linked source: " + str(path))
    require(path.stat().st_size == expected["bytes"] and digest(path) == expected["sha256"],
            "Source size/SHA mismatch: " + str(path))


def image_dimensions(path, kind):
    with path.open("rb") as source:
        header = source.read(8192)
    if kind == "png":
        require(header[:8] == b"\x89PNG\r\n\x1a\n" and header[8:16] == b"\x00\x00\x00\rIHDR",
                "Invalid PNG header: " + str(path))
        require(len(header) >= 33 and zlib.crc32(header[12:29]) == struct.unpack(">I", header[29:33])[0],
                "Invalid PNG IHDR CRC")
        return list(struct.unpack(">II", header[16:24]))
    require(kind == "radiance_hdr" and header.startswith((b"#?RADIANCE", b"#?RGBE"))
            and b"FORMAT=32-bit_rle_rgbe" in header, "Invalid Radiance HDR header")
    match = re.search(rb"\n-Y (\d+) \+X (\d+)\r?\n", header)
    require(match is not None, "Unsupported Radiance orientation")
    return [int(match.group(2)), int(match.group(1))]


def verify_image(path, expected):
    verify_file(path, expected)
    require(image_dimensions(path, expected["format"]) == expected["expected_dimensions"],
            "Source image dimensions mismatch: " + str(path))


def validate_lock():
    lock = json.loads(MANIFEST.read_text(encoding="utf-8"))
    require(lock["schema_version"] == 1 and lock["license"] == "CC0-1.0"
            and lock["runtime_integrated"] is False, "Invalid source-only scope")
    require(lock["release_tag"] == "ground-sky-sources-v1", "Unexpected source release")
    license_item = lock["license_text"]
    license_path = (ROOT / license_item["path"]).resolve()
    require(ROOT in license_path.parents, "External license path")
    verify_file(license_path, license_item)
    entries = lock["assets"]
    require(len(entries) == 10 and len({a["id"] for a in entries}) == 10, "Expected eight ground/two sky assets")
    require(sum(a["kind"] == "ground" for a in entries) == 8
            and sum(a["kind"] == "sky" for a in entries) == 2, "Invalid ground/sky source counts")
    require({a["role"] for a in entries if a["kind"] == "sky"} == {"age_of_light", "age_of_darkness"},
            "Only two environment profiles are allowed")
    names = set()
    for entry in entries:
        safe_name(entry["id"])
        require(entry["page"] == "https://polyhaven.com/a/" + entry["id"] and entry["authors"],
                "Source provenance missing")
        archive = entry["archive"]
        safe_name(archive["filename"])
        require(archive["url"] == RELEASE_BASE + archive["filename"]
                and archive["filename"] not in names, "Invalid/duplicate project source pack")
        names.add(archive["filename"])
        require(re.fullmatch(r"[0-9a-f]{64}", archive["sha256"]) and archive["bytes"] > 0,
                "Unpinned source pack")
        require(len({f["filename"] for f in entry["files"]}) == len(entry["files"]), "Duplicate source member")
        for file in entry["files"]:
            safe_name(file["filename"])
            url = urllib.parse.urlparse(file["url"])
            require(url.scheme == "https" and url.netloc == "dl.polyhaven.org"
                    and url.path.startswith("/file/ph-assets/"), "Unexpected upstream source host")
            require(re.fullmatch(r"[0-9a-f]{64}", file["sha256"])
                    and re.fullmatch(r"[0-9a-f]{32}", file["upstream_md5"])
                    and 0 < file["bytes"] <= 100_000_000, "Invalid source file lock")
        if entry["kind"] == "ground":
            require(entry["resolution"] == "2k" and len(entry["files"]) == 3
                    and {f["slot"] for f in entry["files"]} == {
                        "base_color", "normal_gl", "occlusion_roughness_metallic"}, "Incomplete PBR channel set")
            require(all(f["format"] == "png" and f["expected_dimensions"] == [2048, 2048]
                        and f["color_space"] == ("sRGB" if f["slot"] == "base_color" else "linear")
                        for f in entry["files"]), "Ground source format/channel convention changed")
            require(len(entry["physical_size_m"]) == 2 and all(0 < n < 100 for n in entry["physical_size_m"]),
                    "Invalid physical texture scale")
        else:
            require(entry["kind"] == "sky" and entry["resolution"] == "8k" and len(entry["files"]) == 1
                    and entry["files"][0]["format"] == "radiance_hdr"
                    and entry["files"][0]["expected_dimensions"] == [8192, 4096], "Invalid sky source")
    return lock, license_path.read_bytes()


def download(url, path, expected):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".partial")
    for attempt in range(3):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            count = 0
            with urllib.request.urlopen(request, timeout=45) as response, temporary.open("wb") as output:
                for block in iter(lambda: response.read(1024 * 1024), b""):
                    count += len(block)
                    require(count <= expected["bytes"], "Download exceeds locked size")
                    output.write(block)
            verify_file(temporary, expected)
            temporary.replace(path)
            return
        except Exception as error:
            temporary.unlink(missing_ok=True)
            if isinstance(error, (ValueError, urllib.error.HTTPError)) and (
                    isinstance(error, ValueError) or error.code == 404):
                raise
            if attempt == 2:
                raise
            time.sleep(1 + attempt)


def receipt(entry):
    return json_bytes({"schema_version": 1, "license": "CC0-1.0",
                       "asset": {k: v for k, v in entry.items() if k != "archive"}})


def make_archive(entry, cache, output, license_bytes):
    """Byte-stable ZIP_STORED: originals, per-asset receipt and CC0 legal text."""
    members = {f["filename"]: (cache / entry["id"] / f["filename"]).read_bytes() for f in entry["files"]}
    members.update({"SOURCE.json": receipt(entry), "CC0-1.0.txt": license_bytes})
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".partial")
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_STORED) as archive:
        for name, data in sorted(members.items()):
            info = zipfile.ZipInfo(safe_name(name), date_time=(2026, 10, 7, 0, 0, 0))
            info.create_system = 3
            info.external_attr = (0o100644 << 16)
            archive.writestr(info, data)
    temporary.replace(output)


def verify_archive(entry, path, license_bytes):
    verify_file(path, entry["archive"])
    expected_members = {f["filename"] for f in entry["files"]} | {"SOURCE.json", "CC0-1.0.txt"}
    with zipfile.ZipFile(path) as archive:
        require(len(archive.namelist()) == len(expected_members) and set(archive.namelist()) == expected_members,
                "Unexpected/duplicate source archive members")
        require(archive.read("SOURCE.json") == receipt(entry) and archive.read("CC0-1.0.txt") == license_bytes,
                "Source pack provenance/license mismatch")
        for file in entry["files"]:
            member = archive.getinfo(file["filename"])
            require(member.file_size == file["bytes"] and member.compress_type == zipfile.ZIP_STORED,
                    "Invalid source member size/compression")
            with archive.open(member) as source:
                sha = hashlib.sha256()
                for block in iter(lambda: source.read(1024 * 1024), b""):
                    sha.update(block)
            require(sha.hexdigest() == file["sha256"], "Source member hash mismatch")


def prepare(fetch=False, allow_upstream=False):
    lock, license_bytes = validate_lock()
    paths = []
    for entry in lock["assets"]:
        archive = entry["archive"]
        path = CACHE / "packs" / archive["filename"]
        if not path.exists():
            require(fetch, "Source pack not cached; use --fetch: " + archive["filename"])
            try:
                download(archive["url"], path, archive)
            except urllib.error.HTTPError as error:
                if error.code != 404 or not allow_upstream:
                    raise
                for file in entry["files"]:
                    original = CACHE / entry["id"] / file["filename"]
                    if not original.exists():
                        download(file["url"], original, file)
                    verify_image(original, file)
                make_archive(entry, CACHE, path, license_bytes)
        verify_archive(entry, path, license_bytes)
        paths.append(path)
    return lock, paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fetch", action="store_true")
    parser.add_argument("--allow-upstream", action="store_true")
    parser.add_argument("--verify-cache", action="store_true")
    args = parser.parse_args()
    validate_lock()
    if args.fetch or args.verify_cache:
        lock, paths = prepare(args.fetch, args.allow_upstream)
        print("PASS: exact original source packs:", len(paths), "original images:",
              sum(len(a["files"]) for a in lock["assets"]))
    else:
        print("PASS: eight PBR/two Pure Sky source locks, physical scales and CC0 notice (no network)")


if __name__ == "__main__":
    main()
