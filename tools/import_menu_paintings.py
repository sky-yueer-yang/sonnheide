#!/usr/bin/env python3
"""Copy the five user-supplied menu paintings unchanged; Python 3.9+, offline."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "assets/manifests/menu_paintings.json"
NAMES = ["ChatGPT Image Oct 6, 2026, 11_51_{} PM.jpg".format(n)
         for n in (13, 17, 22, 26, 30)]


def jpeg_size(data):
    if data[:2] != b"\xff\xd8":
        raise ValueError("Not a JPEG")
    offset = 2
    while offset < len(data):
        if data[offset] != 255:
            raise ValueError("Invalid JPEG marker")
        while data[offset] == 255:
            offset += 1
        marker = data[offset]
        offset += 1
        if marker in (0xd8, 0xd9) or 0xd0 <= marker <= 0xd7:
            continue
        size = struct.unpack(">H", data[offset:offset + 2])[0]
        if marker in (0xc0, 0xc1, 0xc2):
            height, width = struct.unpack(">HH", data[offset + 3:offset + 7])
            return width, height
        offset += size
    raise ValueError("JPEG has no supported size marker")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verify():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert manifest["license"] is None, "Do not assign a third-party license to user art"
    assert manifest["source_mode"] == "user_supplied"
    assert len(manifest["paintings"]) == 5
    for index, item in enumerate(manifest["paintings"], 1):
        assert item["id"] == "painting-{:02}".format(index)
        assert item["original_filename"] == NAMES[index - 1]
        path = ROOT / item["path"]
        assert ROOT in path.resolve().parents, "Painting path leaves the project"
        data = path.read_bytes()
        assert len(data) == item["bytes"] and sha(data) == item["sha256"], "Painting bytes changed"
        assert list(jpeg_size(data)) == item["size_px"]
    return "5 user-supplied menu paintings, exact JPEG bytes, dimensions and provenance"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-directory", type=Path)
    args = parser.parse_args()
    if args.source_directory is None:
        print("PASS: " + verify())
        return
    folder = ROOT / "assets/source/ui/paintings"
    folder.mkdir(parents=True, exist_ok=True)
    records = []
    for index, name in enumerate(NAMES, 1):
        source = args.source_directory / name
        data = source.read_bytes()
        width, height = jpeg_size(data)
        target = folder / "painting-{:02}.jpg".format(index)
        shutil.copyfile(source, target)
        records.append({"id": target.stem, "path": str(target.relative_to(ROOT)),
                        "original_filename": name, "bytes": len(data),
                        "sha256": sha(data), "size_px": [width, height]})
    payload = {"schema_version": 1, "status": "local_browser_presentation_assets_not_native_integration",
               "source_mode": "user_supplied", "source_directory_label": "Downloads/油画",
               "permission_basis": "User explicitly requested these paintings for the Sonnheide menu; existing project instruction hosts necessary source files on GitHub.",
               "license": None, "attribution": "No artist or third-party license asserted from filenames.",
               "processing": "Original JPEG bytes copied unchanged. Darkness, cropping, movement and crossfade are runtime presentation.",
               "recipe_tool": "tools/import_menu_paintings.py", "paintings": records}
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print("PASS: " + verify())


if __name__ == "__main__":
    main()
