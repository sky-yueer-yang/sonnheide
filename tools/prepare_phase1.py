#!/usr/bin/env python3
"""Prepare the actual native phase-one resources from locked project sources.

Python 3.9+, standard library. --offline never opens a URL: all original packs
must be present and SHA verified. Warm-cache builds validate generated hashes
without decoding 8K skies or invoking shaderc again. Missing generated resources
are rebuilt; changed/corrupted outputs are rejected until --rebuild is explicit.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import platform
import subprocess
import sys
import prepare_geography as geography
import prepare_ground_sources as ground

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / ".build/ground-sky-runtime"
SHADERS = ROOT / ".build/ground-sky-shaders"
ADMISSION = ROOT / ".build/phase1/ADMITTED.json"


def digest(path):
    return ground.digest(path)


def relative_name(path, base):
    """Serialize native or PureWindowsPath relative names in canonical POSIX form."""
    name = path.relative_to(base).as_posix()
    relative_parts(name)
    return name


def relative_parts(name):
    """Admit one portable relative filename, never a native Windows path/ADS."""
    if (not isinstance(name, str) or not name or "\\" in name
            or any(ord(char) < 32 for char in name)):
        raise RuntimeError("Unsafe generated resource path")
    posix = PurePosixPath(name)
    windows = PureWindowsPath(name)
    parts = posix.parts
    if (posix.is_absolute() or windows.drive or windows.root
            or posix.as_posix() != name or not parts
            or any(part in (".", "..") or ":" in part
                   or any(char in '<>"|?*' for char in part)
                   or part.endswith((" ", ".")) or PureWindowsPath(part).is_reserved()
                   for part in parts)):
        raise RuntimeError("Unsafe generated resource path")
    return parts


def generated_path(base, name, require_build=False):
    """Resolve an admitted name only after rejecting symlinks in its full chain."""
    parts = relative_parts(name)
    if require_build and (parts[0] != ".build" or len(parts) < 2):
        raise RuntimeError("Unsafe generated resource path")
    # ROOT is resolved when this module is loaded. Do not resolve the candidate
    # first: doing so would hide symlinks that happen to target another cache file.
    path = base.joinpath(*parts)
    try:
        path.relative_to(ROOT)
    except ValueError as error:
        raise RuntimeError("Unsafe generated resource path") from error
    cursor = path
    while cursor != ROOT:
        if cursor.is_symlink():
            raise RuntimeError("Symlink in generated resource path")
        cursor = cursor.parent
    resolved = path.resolve()
    resolved_base = base.resolve()
    if resolved_base not in resolved.parents or ROOT not in resolved.parents:
        raise RuntimeError("Unsafe generated resource path")
    return resolved


def backend():
    system = platform.system()
    if system not in ("Darwin", "Windows"):
        raise RuntimeError("Phase-one GPU cooking currently targets Metal or Direct3D11")
    return "metal" if system == "Darwin" else "dx11"


def dependencies():
    paths = [ROOT / "tools/cook_ground_sky.py", ROOT / "tools/prepare_phase1.py",
             ROOT / "assets/manifests/ground_sky_sources.json",
             ROOT / "assets/manifests/ground_sky_runtime.json", ROOT / "data/geo/gshhg_sources.lock.json",
             ROOT / "data/native_dependencies.lock.json",
             ROOT / "presentation/native/earth_renderer.cpp",
             ROOT / "presentation/native/earth_renderer.hpp",
             ROOT / "presentation/native/earth_camera.hpp",
             ROOT / "platform/src/image_decode_bimg.cpp"]
    paths += sorted(p for p in (ROOT / "presentation/native/shaders").iterdir()
                    if p.suffix in (".sc", ".sh"))
    paths += sorted(p for p in (ROOT / "third_party/hextile").rglob("*") if p.is_file())
    if not (ROOT / "presentation/native/shaders/earth_ground.vs.sc").is_file():
        raise RuntimeError("Native Earth shader sources are missing")
    return {relative_name(p, ROOT): digest(p) for p in paths}


def fingerprint(deps, target):
    value = {"schema": 1, "backend": target, "dependencies": deps}
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def output_item(path):
    name = relative_name(path, ROOT)
    checked = generated_path(ROOT, name, require_build=True)
    return {"path": name, "bytes": checked.stat().st_size, "sha256": digest(checked)}


def validate_outputs(items, missing_ok=False):
    missing = False
    for item in items:
        path = generated_path(ROOT, item["path"], require_build=True)
        if not path.exists() and missing_ok:
            missing = True
            continue
        if (not path.is_file() or path.is_symlink() or path.stat().st_size != item["bytes"]
                or digest(path) != item["sha256"]):
            raise RuntimeError("Generated phase-one bytes changed: " + str(path)
                               + "; inspect or use --rebuild to explicitly regenerate")
    return not missing


def prepared_outputs(target):
    runtime_path = generated_path(RUNTIME, "runtime.json")
    runtime = json.loads(runtime_path.read_text(encoding="utf-8"))
    if (runtime["schema_version"] != 1 or len(runtime["material_ids"]) != 8
            or len(runtime["sky_ids"]) != 2
            or runtime["source_lock_sha256"] != digest(ground.MANIFEST)):
        raise RuntimeError("Incomplete native ground/sky runtime manifest")
    resource_header = generated_path(RUNTIME, "earth_resource_lock.hpp")
    if not resource_header.is_file():
        raise RuntimeError("Compiled native resource hash lock header missing")
    outputs = [output_item(runtime_path), output_item(resource_header)]
    for item in runtime["outputs"]:
        path = generated_path(RUNTIME, item["path"])
        if not path.is_file() or digest(path) != item["sha256"]:
            raise RuntimeError("Cooked ground/sky output SHA mismatch")
        outputs.append(output_item(path))
    for asset in runtime["sky_ids"]:
        asset_parts = relative_parts(asset["id"])
        if len(asset_parts) != 1:
            raise RuntimeError("Unsafe sky asset identifier")
        receipt = generated_path(RUNTIME, asset["id"] + "/recipe.json")
        if not receipt.is_file():
            raise RuntimeError("Sky cook recipe receipt is missing")
        outputs.append(output_item(receipt))
    shader_receipt = generated_path(SHADERS, "compiled_shaders.json")
    if not shader_receipt.is_file():
        raise RuntimeError("Compiled native shader provenance receipt missing")
    outputs.append(output_item(shader_receipt))
    for name in ("earth_ground", "earth_sky", "earth_map", "earth_atlas"):
        for stage in ("vs", "fs"):
            path = generated_path(SHADERS, target + "/" + name + "." + stage + ".bin")
            if not path.is_file() or path.stat().st_size < 16:
                raise RuntimeError("Actual compiled native Earth shader missing: " + str(path))
            outputs.append(output_item(path))
    return outputs


def admitted_shader_cache(target):
    """Reuse exact compiled stages after C++/metadata-only presentation changes."""
    import cook_ground_sky as cooker
    receipt_path = generated_path(SHADERS, "compiled_shaders.json")
    if not receipt_path.is_file():
        return False
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    for item in receipt.get("outputs", []):
        relative_parts(item["path"])
    if (receipt.get("sources") != cooker.shader_sources()
            or receipt.get("compile_ast_sha256") != cooker.function_ast_hash({"cook_shaders"})
            or len(receipt.get("outputs", [])) != 8
            or any(not item["path"].startswith(target + "/") for item in receipt["outputs"])):
        return False
    for item in receipt["outputs"]:
        path = generated_path(SHADERS, item["path"])
        if not path.exists():
            return False
        if (path.is_symlink() or not path.is_file() or path.stat().st_size != item["bytes"]
                or digest(path) != item["sha256"]):
            raise RuntimeError("Compiled shader cache bytes changed; inspect or use --rebuild")
    cooker.admitted_shaders(SHADERS)
    return True


def prepare(offline=False, allow_upstream=False, rebuild=False):
    target = backend()
    admission = generated_path(ROOT, relative_name(ADMISSION, ROOT), require_build=True)
    lock = geography.validate_lock()
    if offline and not (geography.CACHE / lock["archive"]["filename"]).is_file():
        raise RuntimeError("Complete offline GSHHG ZIP missing; prepare it online once first")
    # Offline preconditions are checked before the function which may download.
    # Thus even an empty/partially deleted offline cache can never open a URL.
    geography.prepare(fetch=True, allow_upstream=allow_upstream and not offline)
    ground.prepare(fetch=not offline, allow_upstream=allow_upstream and not offline)
    deps = dependencies()
    key = fingerprint(deps, target)
    if admission.is_file() and not rebuild:
        previous = json.loads(admission.read_text(encoding="utf-8"))
        if (previous.get("fingerprint") == key and previous.get("schema_version") == 1
                and previous.get("backend") == target and previous.get("dependencies") == deps
                and previous.get("geography_source_sha256") == lock["archive"]["sha256"]
                and len(previous.get("outputs", [])) >= 37):
            if validate_outputs(previous["outputs"], missing_ok=True):
                print("PASS: SHA-verified phase-one runtime cache; no 8K recook or shader recompilation")
                return previous
    if rebuild:
        # Preserve all original packs; remove only derived outputs from this task.
        import shutil
        for path in (RUNTIME, SHADERS / target):
            if path.exists():
                shutil.rmtree(path)
    # Compile the current source before cooking its presentation recipe and
    # compile-time hash header; locking stale shaders would reject the new build.
    if not admitted_shader_cache(target):
        subprocess.run([sys.executable, str(ROOT / "tools/cook_ground_sky.py"),
                        "--shaders-only", "--shader-output", str(SHADERS)], cwd=ROOT, check=True)
    else:
        print("PASS: exact shader-source/compile-AST/binary cache; compiler skipped")
    subprocess.run([sys.executable, str(ROOT / "tools/cook_ground_sky.py"),
                    "--output", str(RUNTIME), "--shader-output", str(SHADERS)], cwd=ROOT, check=True)
    if dependencies() != deps:
        raise RuntimeError("Phase-one source recipes changed during cooking; rerun after edits finish")
    receipt = {"schema_version": 1, "backend": target, "fingerprint": key,
               "dependencies": deps, "geography_source_sha256": lock["archive"]["sha256"],
               "outputs": prepared_outputs(target)}
    validate_outputs(receipt["outputs"])
    admission.parent.mkdir(parents=True, exist_ok=True)
    partial = generated_path(ROOT, relative_name(admission.with_suffix(".partial"), ROOT), require_build=True)
    partial.write_text(json.dumps(receipt, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    partial.replace(admission)
    print("PASS: native phase-one resources cooked, SHA verified and admitted", key)
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--allow-upstream", action="store_true")
    parser.add_argument("--rebuild", action="store_true")
    args = parser.parse_args()
    prepare(args.offline, args.allow_upstream, args.rebuild)


if __name__ == "__main__":
    main()
