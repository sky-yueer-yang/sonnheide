#!/usr/bin/env python3
"""Validate implemented source/catalog/graybox contracts, not the entire game."""
import base64
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import import_etopo_sample
import import_makehuman_sources
import validate_interaction_schema
import validate_ui_locales
import import_menu_paintings

ROOT = Path(__file__).resolve().parents[1]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def local_path(value):
    path = (ROOT / value).resolve()
    require(ROOT in path.parents and path.is_file(), "Missing or external path: " + value)
    return path


def validate_manifest(manifest):
    require(manifest["authorship"]["mode"] == "original", "All building geometry must be original")
    require(manifest["production_art"] is False and manifest["stage"] == "pipeline_fixture", "Graybox cannot claim production quality")
    source, generated, tool = [local_path(manifest[k]) for k in ("source", "generated", "recipe_tool")]
    for key, path in [("source", source), ("generated", generated), ("recipe_tool", tool)]:
        require(sha(path) == manifest[key + "_sha256"], "Hash mismatch: " + str(path))
    recipe, gltf = read(source), read(generated)
    authority = manifest["authority"]
    require(recipe["authority"] == authority, "Appearance changed authority contract")
    canonical = json.dumps(authority, sort_keys=True, ensure_ascii=False, separators=(",", ":")).encode()
    require(hashlib.sha256(canonical).hexdigest() == manifest["authority_sha256"], "Authority hash mismatch")
    require(manifest["pivot"] == "footprint_center_ground" and manifest["coordinate_system"] == "RH_Y_UP_METERS", "Invalid coordinates")
    footprint = authority["footprint_cells"]
    require(len(footprint) == 2 and all(isinstance(n, int) and n > 0 for n in footprint), "Invalid footprint")
    require(authority["rotations"] == [0, 90, 180, 270], "Rotation contract mismatch")
    if authority["building_type"] == "port":
        require(authority["allowed_origins"] == ["RECLAIMED"] and authority["sea_edge"] == "EAST" and authority["berth_depth_cells"] == 2, "Invalid port geometry contract")
    half = [n * authority["cell_size_m"] / 2 for n in footprint]
    lower, upper = manifest["bounds_m"]["min"], manifest["bounds_m"]["max"]
    require(lower[0] >= -half[0] - 1e-5 and upper[0] <= half[0] + 1e-5 and lower[2] >= -half[1] - 1e-5 and upper[2] <= half[1] + 1e-5 and lower[1] >= -1e-5, "Building spills outside its authority footprint")
    entrance = authority["entrance_m"]
    require(abs(entrance[0]) <= half[0] and abs(entrance[2]) <= half[1] and entrance[1] == 0 and (abs(entrance[0]) == half[0] or abs(entrance[2]) == half[1]), "Entrance is not aligned to boundary")
    require(gltf["asset"]["version"] == "2.0" and not gltf.get("extensionsRequired") and gltf["extras"]["production_art"] is False, "Unsupported glTF profile")
    require(len(gltf["accessors"]) == 4 and len(gltf["bufferViews"]) == 4, "Unexpected fixture accessor profile")
    require(len(gltf["buffers"]) == 1, "Fixture expects one self-contained buffer")
    buffer = gltf["buffers"][0]
    prefix = "data:application/octet-stream;base64,"
    require(buffer["uri"].startswith(prefix), "glTF refers to external data")
    binary = base64.b64decode(buffer["uri"][len(prefix):], validate=True)
    require(len(binary) == buffer["byteLength"], "Buffer length mismatch")
    arrays = []
    for accessor, width, component, code in zip(gltf["accessors"], [3, 3, 2, 1], [5126, 5126, 5126, 5125], ["f", "f", "f", "I"]):
        require(accessor["componentType"] == component, "Wrong accessor type")
        view = gltf["bufferViews"][accessor["bufferView"]]
        start, length = view["byteOffset"], view["byteLength"]
        require(start % 4 == 0 and length == accessor["count"] * width * 4 and start + length <= len(binary), "Invalid accessor range")
        values = struct.unpack("<" + code * accessor["count"] * width, binary[start:start + length])
        require(all(math.isfinite(n) for n in values), "Nonfinite vertex data")
        arrays.append([values[i:i+width] for i in range(0, len(values), width)])
    positions, normals, _, index_rows = arrays
    indices = [r[0] for r in index_rows]
    require(len(indices) // 3 == manifest["triangles"] and all(i < len(positions) for i in indices), "Invalid triangle indices")
    for offset in range(0, len(indices), 3):
        ids = indices[offset:offset+3]
        a, b, c = [positions[i] for i in ids]
        ab, ac = [b[i]-a[i] for i in range(3)], [c[i]-a[i] for i in range(3)]
        cross = [ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0]]
        require(sum(cross[i]*normals[ids[0]][i] for i in range(3)) > 0, "Degenerate or inward triangle")
    for i in range(3):
        require(abs(min(p[i] for p in positions) - lower[i]) < 1e-5 and abs(max(p[i] for p in positions) - upper[i]) < 1e-5, "False mesh bounds")


def validate_interface_assets():
    manifest = read(ROOT / "assets/manifests/interface_assets.json")
    logo = manifest["branding"]
    require(logo["source_mode"] == "user_supplied" and logo["license"] is None,
            "Company mark must not acquire an invented open license")
    path = local_path(logo["path"])
    require(path.stat().st_size == logo["bytes"] and sha(path) == logo["sha256"],
            "Supplied company mark bytes changed")
    require(path.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n", "Company mark is not PNG")
    fonts = manifest["fonts"]
    require(len(fonts) == 1 and fonts[0]["family"] == "Cinzel" and
            fonts[0]["license"] == "OFL-1.1" and fonts[0]["modified"] is False and
            fonts[0]["commit"] == "3dd78844021e948ceb633d1dcee3f7885561b5d9",
            "Menu font source lock changed")
    for item in fonts[0]["files"]:
        path = local_path(item["path"])
        require(path.stat().st_size == item["bytes"] and sha(path) == item["sha256"],
                "Pinned font or license bytes changed: " + item["path"])
        require(item["url"].startswith("https://raw.githubusercontent.com/google/fonts/" +
                fonts[0]["commit"] + "/ofl/cinzel/"), "Font source is not pinned official repository")
    require({i["upstream_path"] for i in fonts[0]["files"]} ==
            {"ofl/cinzel/Cinzel[wght].ttf", "ofl/cinzel/OFL.txt", "ofl/cinzel/METADATA.pb"},
            "Font license or metadata missing")
    return "unchanged supplied company mark and locally served Cinzel/OFL source hashes"


def main():
    dependencies = read(ROOT / "data/dependencies.json")
    for source in dependencies["retained_geographic_sources"]:
        require(sha(local_path(source["license_file"])) == source["license_sha256"], "Geographic license text changed")
    ocean = ROOT / "third_party/abyssal-ocean"
    upstream = read(ocean / "UPSTREAM.json")
    require(upstream["commit"] == "142265f5013b6f27bea4f4f819b832dec75c7bad" and upstream["license"] == "MIT", "Abyssal source lock changed")
    require({f["path"] for f in upstream["files"]} == {"LICENSE", "README.md", "index.html"}, "Abyssal source set changed")
    for item in upstream["files"]:
        path = local_path("third_party/abyssal-ocean/" + item["path"])
        require(path.stat().st_size == item["bytes"] and sha(path) == item["sha256"], "Abyssal exact source bytes changed: " + item["path"])
    for path in upstream["derived_files"]:
        local_path(path)
    bundle = import_etopo_sample.BUNDLE_PATH.read_bytes()
    metadata = import_etopo_sample.METADATA_PATH.read_bytes()
    rebuilt = import_etopo_sample.json_bytes(import_etopo_sample.make_sample(bundle, metadata))
    require(rebuilt == import_etopo_sample.SAMPLE_PATH.read_bytes(), "Real ETOPO sample differs from offline source rebuild")
    provenance = read(ROOT / "data/catalogs/design_source.json")
    require(sha(local_path(provenance["source"])) == provenance["sha256"], "Design baseline changed without regeneration")
    catalog = read(ROOT / "data/catalogs/technology.json")
    require([len(catalog[k]) for k in ("capabilities", "families", "slots")] == [40, 96, 288], "Technology counts differ")
    items = catalog["capabilities"] + catalog["families"] + catalog["slots"]
    require(len({v["id"] for v in items}) == 424, "Duplicate definition IDs")
    capabilities = {c["id"]: c for c in catalog["capabilities"]}
    active, done = set(), set()

    def visit(key):
        require(key in capabilities, "Unknown prerequisite: " + key)
        require(key not in active, "Capability dependency cycle")
        if key in done:
            return
        active.add(key)
        for parent in capabilities[key]["prerequisite_ids"]:
            visit(parent)
        active.remove(key)
        done.add(key)
    for key in capabilities:
        visit(key)
    slots = {s["id"]: s for s in catalog["slots"]}
    for family in catalog["families"]:
        require(len(family["slot_ids"]) == 3, "Family requires three slots")
        for key in family["slot_ids"]:
            require(slots[key]["family_id"] == family["id"], "Slot belongs to wrong family")
        require(all(p in capabilities for p in family["prerequisite_ids"]), "Unknown family prerequisite")
    for name, field, expected in [("laws.json", "laws", 35), ("businesses.json", "businesses", 13), ("commands.json", "commands", 23)]:
        data = read(ROOT / "data/catalogs" / name)
        require(len(data[field]) == expected and data["provenance"] == provenance, "Catalog provenance/count mismatch")
    chapters = read(ROOT / "data/requirements/chapters.json")["chapters"]
    require([c["chapter"] for c in chapters] == list(range(1, 28)), "All 27 chapters must be tracked")
    for chapter in chapters:
        require(chapter["status"] in ["planned", "partial_kernel"], "False implementation claim")
        local_path(chapter["architecture_document"])
    manifests = sorted((ROOT / "assets/manifests").glob("*_graybox.json"))
    require(len(manifests) == 3, "Expected three original pipeline fixtures")
    for manifest in manifests:
        validate_manifest(read(manifest))
    character = dependencies["retained_character_sources"][0]
    require(character["source_manifest"] == import_makehuman_sources.MANIFEST and character["game_ready"] is False and character["program_code_imported"] is False, "MakeHuman source scope mismatch")
    import_makehuman_sources.verify(ROOT)
    interaction_summary = validate_interaction_schema.validate(ROOT)
    ui_summary = validate_ui_locales.validate(ROOT)
    painting_summary = import_menu_paintings.verify()
    interface_summary = validate_interface_assets()
    # Check actual relative Markdown links, excluding URLs, anchors and inline examples.
    for path in [ROOT / "README.md", *sorted((ROOT / "docs").rglob("*.md"))]:
        if path == ROOT / provenance["source"]:
            continue  # The unchanged source describes absent attachments; preserve it verbatim.
        for target in re.findall(r"\]\(([^)]+)\)", path.read_text(encoding="utf-8")):
            if "://" in target or target.startswith("#"):
                continue
            base = target.split("#", 1)[0]
            require((path.parent / base).exists(), "Broken local link in {}: {}".format(path, target))
    print("PASS: pinned Abyssal MIT sources, offline real ETOPO sample, baseline SHA-256, 424 technology definitions and DAG, 35 laws, 13 businesses, 23 commands, 27 chapters, 3 original glTF contracts, selected MakeHuman sources, documentation links")
    print("PASS: " + interaction_summary)
    print("PASS: " + ui_summary)
    print("PASS: " + painting_summary)
    print("PASS: " + interface_summary)



if __name__ == "__main__":
    main()
