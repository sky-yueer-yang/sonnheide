#!/usr/bin/env python3
"""Original procedural fixtures. No imported building geometry or engine dependency."""
import base64
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def generate(source):
    raw = source.read_bytes()
    recipe = json.loads(raw)
    positions, normals, uvs, indices = [], [], [], []
    # Outward CCW quad winding. Each face has its own vertices/normals/UV.
    faces = [((1, 0, 0), (1, 3, 7, 5)), ((-1, 0, 0), (0, 4, 6, 2)),
             ((0, 1, 0), (2, 6, 7, 3)), ((0, -1, 0), (0, 1, 5, 4)),
             ((0, 0, 1), (4, 5, 7, 6)), ((0, 0, -1), (0, 2, 3, 1))]
    for box in recipe["boxes"]:
        center, size = box["center"], box["size"]
        corners = [(center[0] + (1 if n & 1 else -1) * size[0] / 2,
                    center[1] + (1 if n & 2 else -1) * size[1] / 2,
                    center[2] + (1 if n & 4 else -1) * size[2] / 2) for n in range(8)]
        for normal, face in faces:
            first = len(positions)
            positions.extend(corners[n] for n in face)
            normals.extend([normal] * 4)
            uvs.extend([(0, 0), (1, 0), (1, 1), (0, 1)])
            indices.extend([first, first + 1, first + 2, first, first + 2, first + 3])
    floats = lambda rows: struct.pack("<" + "f" * sum(len(row) for row in rows), *[x for row in rows for x in row])
    chunks = [floats(positions), floats(normals), floats(uvs), struct.pack("<" + "I" * len(indices), *indices)]
    blob = b"".join(chunks)
    minimum = [min(p[i] for p in positions) for i in range(3)]
    maximum = [max(p[i] for p in positions) for i in range(3)]
    views, offset = [], 0
    for n, chunk in enumerate(chunks):
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(chunk), "target": 34963 if n == 3 else 34962})
        offset += len(chunk)
    accessors = [{"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": minimum, "max": maximum},
                 {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
                 {"bufferView": 2, "componentType": 5126, "count": len(uvs), "type": "VEC2"},
                 {"bufferView": 3, "componentType": 5125, "count": len(indices), "type": "SCALAR"}]
    doc = {"asset": {"version": "2.0", "generator": "Sonnheide original graybox generator v1"},
           "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": recipe["asset_id"], "mesh": 0}],
           "buffers": [{"byteLength": len(blob), "uri": "data:application/octet-stream;base64," + base64.b64encode(blob).decode("ascii")}],
           "bufferViews": views, "accessors": accessors,
           "materials": [{"name": "original_graybox", "pbrMetallicRoughness": {"baseColorFactor": [0.55, 0.57, 0.6, 1], "metallicFactor": 0, "roughnessFactor": 0.85}}],
           "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0, "mode": 4}]}],
           "extras": {"production_art": False, "authorship": "original Sonnheide geometry", "unit": "meter", "pivot": "footprint_center_ground"}}
    destination = ROOT / "assets/generated/buildings" / (recipe["asset_id"] + ".gltf")
    encoded = (json.dumps(doc, ensure_ascii=False, indent=2) + "\n").encode()
    destination.write_bytes(encoded)
    authority = recipe["authority"]
    canonical = json.dumps(authority, sort_keys=True, ensure_ascii=False, separators=(",", ":")).encode()
    manifest = {"schema_version": 1, "asset_id": recipe["asset_id"], "kind": "building_graybox",
                "authorship": {"mode": "original", "project": "Sonnheide"}, "production_art": False,
                "source": source.relative_to(ROOT).as_posix(), "source_sha256": digest(raw), "recipe_tool": "tools/build_grayboxes.py",
                "recipe_tool_sha256": digest(Path(__file__).read_bytes()), "generated": destination.relative_to(ROOT).as_posix(),
                "generated_sha256": digest(encoded), "authority": authority, "authority_sha256": digest(canonical),
                "coordinate_system": "RH_Y_UP_METERS", "pivot": "footprint_center_ground", "bounds_m": {"min": minimum, "max": maximum},
                "triangles": len(indices) // 3, "draw_calls": 1, "stage": "pipeline_fixture"}
    (ROOT / "assets/manifests" / (recipe["asset_id"] + ".json")).write_bytes((json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))


def main():
    sources = sorted((ROOT / "assets/source/buildings").glob("*.json"))
    for source in sources:
        generate(source)
    print("Generated {} original building fixtures and provenance manifests".format(len(sources)))


if __name__ == "__main__":
    main()
