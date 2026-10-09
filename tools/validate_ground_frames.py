#!/usr/bin/env python3
"""Validate texture contribution in the formal native World capture, using Python 3.9+ stdlib.

Run Sonnheide --phase1-smoke --capture-dir <ignored QA directory> first. This
reads actual bgfx screenshots; neither file hashes nor standalone probe images
are substitutes for the same-camera, same-World channel comparisons below.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct


def load_tga(path):
    data = path.read_bytes()
    if len(data) < 18:
        raise ValueError("Truncated screenshot: " + str(path))
    image_id, color_map, image_type = data[:3]
    width, height = struct.unpack_from("<HH", data, 12)
    bits, descriptor = data[16:18]
    if (color_map or image_type != 2 or bits != 32 or descriptor & 16
            or not width or not height or width * height > 64_000_000):
        raise ValueError("Expected bgfx uncompressed BGRA32 screenshot: " + str(path))
    offset = 18 + image_id
    if len(data) != offset + width * height * 4:
        raise ValueError("Incomplete screenshot: " + str(path))
    rows = [data[offset + y * width * 4:offset + (y + 1) * width * 4] for y in range(height)]
    if not descriptor & 32:
        rows.reverse()
    return width, height, b"".join(rows)


def comparison(full, other, width, height):
    absolute = changed = maximum = 0
    min_x, min_y, max_x, max_y = width, height, -1, -1
    border_absolute = border_count = 0
    # The fixed uppermost strip is sky plus the unchanged title panel in the
    # formal low-angle camera. It is reported separately, never used as land.
    border_height = max(1, height // 32)
    for pixel in range(width * height):
        offset = pixel * 4
        diffs = [abs(full[offset + channel] - other[offset + channel]) for channel in range(3)]
        total = sum(diffs)
        absolute += total
        maximum = max(maximum, *diffs)
        y, x = divmod(pixel, width)
        if y < border_height:
            border_absolute += total
            border_count += 3
        if total:
            changed += 1
            min_x, min_y = min(min_x, x), min(min_y, y)
            max_x, max_y = max(max_x, x), max(max_y, y)
    return {
        "mean_absolute_rgb_difference_8bit": absolute / (width * height * 3),
        "maximum_channel_difference_8bit": maximum,
        "changed_pixel_count": changed,
        "changed_pixel_fraction": changed / (width * height),
        "changed_bounds": None if not changed else [min_x, min_y, max_x + 1, max_y + 1],
        "top_strip_mean_absolute_rgb_difference_8bit": border_absolute / border_count,
    }


def validate(folder):
    evidence = json.loads((folder / "ground-camera.json").read_text(encoding="utf-8"))
    camera = evidence["camera"]
    if (evidence.get("schema_version") != 1 or evidence.get("entry") != "formal-world"
            or evidence.get("backend") not in ("Metal", "Direct3D 11", "Direct3D11")):
        raise ValueError("Evidence must come from the actual formal native World on Metal or D3D11")
    if (camera.get("orthographic") is not False
            or not all(math.isfinite(camera[key]) for key in
                       ("height_m", "pitch_deg", "yaw_deg", "target_x_m", "target_z_m"))
            or not 0 < camera["height_m"] <= 40 or not 0 < camera["pitch_deg"] <= 25):
        raise ValueError("Formal entry did not use the required finite perspective close camera")
    for key, length in (("world_id", 32), ("resource_recipe_hash", 64)):
        value = evidence.get(key, "")
        if len(value) != length or any(char not in "0123456789abcdef" for char in value):
            raise ValueError("Missing canonical " + key)
    names = ["ground-full", "ground-mean-base", "ground-flat-normal", "ground-mean-arm", "ground-restored"]
    if evidence.get("frames") != names:
        raise ValueError("Missing formal per-channel capture sequence")
    frames = {}
    for name in names:
        path = folder / (name + ".tga")
        if hashlib.sha256(path.read_bytes()).hexdigest() != evidence["frame_file_sha256"][name]:
            raise ValueError("Screenshot identity does not match this formal capture session: " + name)
        width, height, pixels = load_tga(path)
        if (width, height) != (evidence["width"], evidence["height"]):
            raise ValueError("Viewport changed during comparison: " + name)
        frames[name] = pixels
    width, height = evidence["width"], evidence["height"]
    full = frames[names[0]]
    results = {name: comparison(full, frames[name], width, height) for name in names[1:]}
    failures = []
    # Eight-bit image quantization is the floor. Base must produce a plainly
    # visible signal over a substantial pixel region. Normal/ARM can be subtle;
    # require distributed nonzero contribution rather than an inflated target.
    base = results["ground-mean-base"]
    if base["mean_absolute_rgb_difference_8bit"] < 0.5 or base["changed_pixel_fraction"] < 0.02:
        failures.append("Base texture does not visibly contribute to the formal ground view")
    for name in ("ground-flat-normal", "ground-mean-arm"):
        item = results[name]
        if item["mean_absolute_rgb_difference_8bit"] <= 0 or item["changed_pixel_count"] < 256:
            failures.append(name + " has no distributed measurable contribution")
    restored = results["ground-restored"]
    if restored["changed_pixel_count"]:
        failures.append("Restored Full frame differs: lighting/UI/camera or capture sequence drifted")
    for name in names[1:4]:
        if results[name]["top_strip_mean_absolute_rgb_difference_8bit"] > 0.1:
            failures.append(name + " changed the fixed sky/UI strip; comparison is not isolated")
    report = {
        "schema_version": 1,
        "passed": not failures,
        "world_id": evidence["world_id"],
        "resource_recipe_hash": evidence["resource_recipe_hash"],
        "backend": evidence["backend"],
        "camera": camera,
        "width": width, "height": height,
        "comparisons": results,
        "framebuffer_sha256": {name: hashlib.sha256(pixels).hexdigest() for name, pixels in frames.items()},
        "failures": failures,
        "scope": "formal World entry; one ground texture channel rebound at a time, same World/camera/UI/light; no global-scale detail claim",
    }
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_directory", type=Path)
    parser.add_argument("--report", type=Path, help="Optional ignored JSON evidence output")
    args = parser.parse_args()
    try:
        report = validate(args.capture_directory)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, "Ground capture validation failed: " + str(error) + "\n")
    text = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.report:
        args.report.write_text(text, encoding="utf-8")
    print(text, end="")
    if not report["passed"]:
        parser.exit(1)


if __name__ == "__main__":
    main()
