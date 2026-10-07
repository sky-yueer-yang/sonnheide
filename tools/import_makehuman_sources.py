#!/usr/bin/env python3
"""Verify the selected MakeHuman source set offline; --fetch restores pinned bytes.

Python 3.9+ standard library only. This does not execute MakeHuman, Blender or
MPFB, fit clothing to arbitrary shapes, or export a runtime character.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys
import urllib.request
import uuid
import zlib


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = "assets/manifests/makehuman_sources.json"
SOURCE_PREFIX = "assets/source/third_party/makehuman/"
CORE_COMMIT = "1f508f6083b2f823dab15de924b3bde72e08d77c"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def finite(value):
    result = float(value)
    require(math.isfinite(result), "non-finite graphical value")
    return result


def read_manifest(root):
    manifest = json.loads((root / MANIFEST).read_text(encoding="utf-8"))
    require(manifest["schema_version"] == 1, "unsupported source manifest")
    require(manifest["makehuman"]["commit"] == CORE_COMMIT, "unreviewed core commit")
    require(len(manifest["files"]) == 14, "unexpected selected source inventory")
    require({garment["asset_id"] for garment in manifest["garments"]} == {
        "elvs_crude_t-shirt_male", "joepal_crude_t-shirt_female", "cortu_jeans_shorts"
    } and len(manifest["garments"]) == 3, "unexpected selected garment inventory")
    paths = set()
    for entry in manifest["files"]:
        path = entry["path"]
        require(path.startswith(SOURCE_PREFIX), "source path outside MakeHuman root")
        require((root / path).resolve().is_relative_to((root / SOURCE_PREFIX).resolve()),
                "unsafe source path")
        require(path not in paths, "duplicate source path")
        paths.add(path)
        require(re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]) is not None,
                "invalid source hash")
        require(0 < entry["size_bytes"] <= 2_000_000, "unexpected source file size")
        require(entry["license"] in ("CC0-1.0", "LicenseRef-MakeHuman-Split-Notice"),
                "unreviewed source license")
        if "upstream_path" in entry:
            expected = "https://raw.githubusercontent.com/makehumancommunity/makehuman/"
            expected += CORE_COMMIT + "/" + entry["upstream_path"]
            require(entry["source_url"] == expected, "unreviewed core source URL")
        else:
            pack = entry["pack"]
            require(pack in ("shirts01", "pants01"), "unreviewed garment pack")
            expected = "https://files.makehumancommunity.org/asset_packs/"
            expected += pack + "/" + pack + "_cc0.zip"
            require(entry["source_url"] == expected, "unreviewed garment source URL")
            require(entry["upstream_member"].startswith("clothes/" + entry["asset_id"] + "/"),
                    "garment member outside selected asset")
    return manifest


def verify_bytes(entry, data):
    require(len(data) == entry["size_bytes"], "source size mismatch: " + entry["path"])
    require(hashlib.sha256(data).hexdigest() == entry["sha256"],
            "source SHA-256 mismatch: " + entry["path"])
    header = ("blob " + str(len(data)) + "\0").encode("ascii")
    require(hashlib.sha1(header + data).hexdigest() == entry["git_blob_sha1"],
            "source Git object mismatch: " + entry["path"])
    data.decode("utf-8", errors="strict")


def fetch_source(entry):
    if "upstream_path" in entry:
        request = urllib.request.Request(entry["source_url"])
        with urllib.request.urlopen(request, timeout=45) as response:
            data = response.read(entry["size_bytes"] + 1)
    else:
        start, end = entry["range_bytes"]
        require(start == entry["header_offset"] and end >= start, "invalid ZIP byte range")
        request = urllib.request.Request(entry["source_url"], headers={
            "Range": "bytes=" + str(start) + "-" + str(end),
            "User-Agent": "Sonnheide-source-import/1.0",
        })
        with urllib.request.urlopen(request, timeout=45) as response:
            require(response.status == 206, "server did not honor pinned ZIP range")
            archive_bytes = response.read(entry["range_size_bytes"] + 1)
            content_range = response.headers.get("Content-Range", "")
            require(content_range.startswith("bytes " + str(start) + "-" + str(end) + "/"),
                    "ZIP range response mismatch")
        require(len(archive_bytes) == entry["range_size_bytes"], "ZIP range size mismatch")
        require(hashlib.sha256(archive_bytes).hexdigest() == entry["range_sha256"],
                "pinned compressed ZIP bytes changed")
        header = struct.unpack_from("<4s5H3L2H", archive_bytes)
        require(header[0] == b"PK\x03\x04" and header[3] == 8, "unexpected ZIP member header")
        name_length, extra_length = header[-2:]
        name = archive_bytes[30:30 + name_length].decode("utf-8")
        require(name == entry["upstream_member"], "ZIP member name mismatch")
        data_offset = 30 + name_length + extra_length
        require(data_offset == entry["local_file_data_offset"], "ZIP member offset mismatch")
        compressed = archive_bytes[data_offset:data_offset + entry["compressed_size"]]
        inflater = zlib.decompressobj(-15)
        data = inflater.decompress(compressed, entry["size_bytes"] + 1)
        require(inflater.eof and not inflater.unconsumed_tail and not inflater.unused_data,
                "invalid or oversized Deflate member")
        require(zlib.crc32(data) == entry["crc32"], "ZIP member CRC mismatch")
    verify_bytes(entry, data)
    return data


def parse_obj(text):
    vertices, uvs, normals, faces = [], [], [], []
    groups = {}
    current_group = "ungrouped"
    for line in text.splitlines():
        parts = line.split()
        if not parts or parts[0].startswith("#"):
            continue
        kind = parts[0]
        if kind == "v":
            require(len(parts) == 4, "unsupported OBJ vertex")
            vertices.append(tuple(finite(x) for x in parts[1:]))
        elif kind == "vt":
            require(len(parts) in (3, 4), "unsupported OBJ UV")
            uvs.append(tuple(finite(x) for x in parts[1:]))
        elif kind == "vn":
            require(len(parts) == 4, "unsupported OBJ normal")
            normals.append(tuple(finite(x) for x in parts[1:]))
        elif kind == "g":
            require(len(parts) == 2, "unsupported multiple OBJ groups")
            current_group = parts[1]
        elif kind == "f":
            require(4 <= len(parts) <= 5, "selected source face must be triangle or quad")
            face = []
            for token in parts[1:]:
                indices = token.split("/")
                require(1 <= len(indices) <= 3 and indices[0], "invalid OBJ face index")
                for index, count in zip(indices, (len(vertices), len(uvs), len(normals))):
                    if index:
                        require(1 <= int(index) <= count, "OBJ index outside source mesh")
                face.append(int(indices[0]) - 1)
            faces.append(face)
            groups[current_group] = groups.get(current_group, 0) + 1
        elif kind not in ("s", "o", "mtllib", "usemtl"):
            raise ValueError("unsupported OBJ statement: " + kind)
    require(vertices and faces, "empty source mesh")
    return {"vertices": vertices, "uv_count": len(uvs), "faces": faces, "groups": groups}


def verify_rig(skeleton, weights, vertex_count):
    require(skeleton["license"] == weights["license"] == "CC0", "rig license mismatch")
    require(skeleton["weights_file"] == "default_weights.mhw", "rig weight reference mismatch")
    bones, joints, planes = skeleton["bones"], skeleton["joints"], skeleton["planes"]
    for indices in joints.values():
        require(indices and all(type(i) is int and 0 <= i < vertex_count for i in indices),
                "joint helper index outside hm08")
    for references in planes.values():
        require(len(references) == 3 and all(j in joints for j in references),
                "rig rotation plane references missing joint")
    roots = []
    for name, bone in bones.items():
        require(bone["head"] in joints and bone["tail"] in joints, "bone endpoint missing joint")
        require(bone["rotation_plane"] in planes, "bone rotation plane missing")
        parent = bone["parent"]
        if parent is None:
            roots.append(name)
        else:
            require(parent in bones, "missing skeleton parent")
        chain, cursor = set(), name
        while cursor is not None:
            require(cursor not in chain, "cycle in skeleton parents")
            chain.add(cursor)
            cursor = bones[cursor]["parent"]
    require(len(roots) == 1, "selected default skeleton must have one root")
    sums = [0.0] * vertex_count
    influences = [[] for _ in range(vertex_count)]
    entry_count = 0
    for bone, values in weights["weights"].items():
        require(bone in bones, "weights reference missing bone")
        for index, weight in values:
            require(type(index) is int and 0 <= index < vertex_count, "weight index outside hm08")
            weight = finite(weight)
            require(0 <= weight <= 1, "source influence outside 0..1")
            sums[index] += weight
            influences[index].append(weight)
            entry_count += 1
    require(all(total > 0 and math.isfinite(total) for total in sums),
            "source weight vertex cannot be normalized")
    max_error = max(abs(math.fsum(w / total for w in values) - 1.0)
                    for total, values in zip(sums, influences))
    require(max_error < 1e-12, "normalized source influences do not sum to one")
    return {"bones": len(bones), "joint_helpers": len(joints), "raw_weight_entries": entry_count,
            "weighted_vertices": vertex_count, "raw_weight_sum_min": min(sums),
            "raw_weight_sum_max": max(sums), "normalized_sum_max_error": max_error}


def parse_mhclo(text, vertex_count):
    require("CC0" in text[:800], "selected garment lacks expected license evidence")
    metadata, mappings, deleted = {}, [], set()
    section = "header"
    for line in text.splitlines():
        parts = line.split()
        if not parts or parts[0].startswith("#"):
            continue
        if parts[0] == "verts":
            require(parts == ["verts", "0"], "unsupported garment mapping offset")
            section = "mappings"
        elif parts[0] == "delete_verts":
            section = "mask"
        elif section == "header":
            require(parts[0] not in metadata, "duplicate garment metadata")
            metadata[parts[0]] = parts[1:]
        elif section == "mappings":
            require(len(parts) == 9, "unsupported garment fitting row")
            indices = [int(x) for x in parts[:3]]
            require(all(0 <= i < vertex_count for i in indices), "garment fit index outside hm08")
            values = [finite(x) for x in parts[3:]]
            # These are signed affine fitting coordinates, not skin influences.
            require(abs(sum(values[:3]) - 1.0) <= 3e-5, "garment fitting coordinates not affine")
            mappings.append((indices, values))
        else:
            cursor = 0
            while cursor < len(parts):
                start = int(parts[cursor])
                end = start
                cursor += 1
                if cursor < len(parts) and parts[cursor] == "-":
                    require(cursor + 1 < len(parts), "truncated body mask range")
                    end = int(parts[cursor + 1])
                    cursor += 2
                require(0 <= start <= end < vertex_count, "body mask outside hm08")
                deleted.update(range(start, end + 1))
    require(metadata["basemesh"] == ["hm08"], "garment uses different basemesh")
    uuid.UUID(metadata["uuid"][0])
    for axis in ("x_scale", "y_scale", "z_scale"):
        values = metadata[axis]
        require(len(values) == 3, "invalid garment axis scale")
        require(all(0 <= int(i) < vertex_count for i in values[:2]), "scale index outside hm08")
        require(finite(values[2]) > 0, "non-positive garment reference scale")
    for key in ("obj_file", "material"):
        require(len(metadata[key]) == 1 and Path(metadata[key][0]).name == metadata[key][0],
                "garment reference outside selected directory")
    return metadata, mappings, deleted


def material_textures(text):
    return sorted(parts[1] for line in text.splitlines() if (parts := line.split())
                  and parts[0].endswith("Texture") and len(parts) == 2)


def verify(root=ROOT):
    """Return a structural source audit summary; never connects to the network."""
    root = Path(root)
    manifest = read_manifest(root)
    for entry in manifest["files"]:
        verify_bytes(entry, (root / entry["path"]).read_bytes())
    core = root / SOURCE_PREFIX / "core"
    body_text = (core / "makehuman/data/3dobjs/base.obj").read_text(encoding="utf-8")
    require("basemesh hm08" in body_text[:800] and "CC0" in body_text[:800],
            "base mesh identity or license mismatch")
    body = parse_obj(body_text)
    require("body" in body["groups"] and "helper-tights" in body["groups"],
            "source body/helpers absent")
    skeleton = json.loads((core / "makehuman/data/rigs/default.mhskel").read_text(encoding="utf-8"))
    weights = json.loads((core / "makehuman/data/rigs/default_weights.mhw").read_text(encoding="utf-8"))
    rig = verify_rig(skeleton, weights, len(body["vertices"]))
    require("CC0 1.0 Universal" in (core / "LICENSE.ASSETS.md").read_text(encoding="utf-8"),
            "missing original CC0 legal text")
    notice = (core / "LICENSE.md").read_text(encoding="utf-8")
    require("third part asset" in notice and "AGPL" in notice, "missing split-license notice")
    garments = []
    for garment in manifest["garments"]:
        require(garment["license"] == "CC0-1.0" and garment["runtime_ready"] is False,
                "selected garment scope or license mismatch")
        directory = root / SOURCE_PREFIX / "clothes" / garment["pack"]
        mhclo_path = directory / garment["mhclo"]
        require(mhclo_path.resolve().is_relative_to(directory.resolve()),
                "garment descriptor outside selected directory")
        metadata, mappings, deleted = parse_mhclo(mhclo_path.read_text(encoding="utf-8"),
                                                 len(body["vertices"]))
        obj = parse_obj((mhclo_path.parent / metadata["obj_file"][0]).read_text(encoding="utf-8"))
        require(len(mappings) == len(obj["vertices"]), "cloth fitting rows/OBJ vertices differ")
        material = (mhclo_path.parent / metadata["material"][0]).read_text(encoding="utf-8")
        for line in material.splitlines():
            parts = line.split()
            if parts and (parts[0].endswith("Color") or parts[0] in (
                    "opacity", "shininess", "translucency", "normalmapIntensity")):
                require(all(math.isfinite(finite(value)) for value in parts[1:]),
                        "invalid material numeric attribute")
        textures = material_textures(material)
        require(textures == sorted(garment["missing_textures"]), "material dependency declaration differs")
        require(all(not (mhclo_path.parent / t).exists() for t in textures),
                "undeclared additional texture import")
        garments.append({"asset_id": garment["asset_id"], "vertices": len(obj["vertices"]),
                         "faces": len(obj["faces"]), "fit_rows": len(mappings),
                         "masked_body_vertices": len(deleted), "missing_textures": textures})
    return {"source_files": len(manifest["files"]),
            "source_bytes": sum(e["size_bytes"] for e in manifest["files"]),
            "basemesh_vertices": len(body["vertices"]), "basemesh_faces": len(body["faces"]),
            "basemesh_uvs": body["uv_count"], "rig": rig, "garments": garments,
            "runtime_ready": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fetch", action="store_true", help="restore exact selected upstream bytes")
    parser.add_argument("--verify", action="store_true", help="verify offline (also the default)")
    args = parser.parse_args()
    if args.fetch:
        manifest = read_manifest(ROOT)
        # Verify every fetched file in memory before replacing the source set.
        fetched = [(entry, fetch_source(entry)) for entry in manifest["files"]]
        for entry, data in fetched:
            target = ROOT / entry["path"]
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    print(json.dumps(verify(ROOT), indent=2))
    print("MakeHuman selected source verification passed; runtime character export pending.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError, struct.error) as error:
        print("MakeHuman source verification failed: " + str(error), file=sys.stderr)
        raise SystemExit(1)
