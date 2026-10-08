#!/usr/bin/env python3
"""Cross-platform phase-one receipt/path regression tests; Python 3.9+ stdlib."""
import ast
import copy
import json
from pathlib import Path, PureWindowsPath
import sys
import tempfile
import unittest
from unittest import mock

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "tools"))
import prepare_phase1 as phase1
import cook_ground_sky as cooker


class ResourcePathTests(unittest.TestCase):
    def setUp(self):
        (PROJECT / ".build").mkdir(exist_ok=True)
        self.directory = tempfile.TemporaryDirectory(prefix="phase1-path-test-", dir=str(PROJECT / ".build"))
        self.root = Path(self.directory.name).resolve()
        self.runtime = self.root / ".build/ground-sky-runtime"
        self.shaders = self.root / ".build/ground-sky-shaders"
        self.runtime.mkdir(parents=True)
        self.shaders.mkdir(parents=True)
        self.patches = [mock.patch.object(phase1, "ROOT", self.root),
                        mock.patch.object(phase1, "RUNTIME", self.runtime),
                        mock.patch.object(phase1, "SHADERS", self.shaders),
                        mock.patch.object(phase1, "ADMISSION", self.root / ".build/phase1/ADMITTED.json")]
        for patch in self.patches:
            patch.start()

    def tearDown(self):
        for patch in reversed(self.patches):
            patch.stop()
        self.directory.cleanup()

    def file(self, relative, content=b"locked-file-fixture"):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def record(self, path):
        return {"path": path.relative_to(self.root).as_posix(),
                "bytes": path.stat().st_size, "sha256": phase1.digest(path)}

    def test_real_pure_windows_path_serializes_forward_slashes(self):
        root = PureWindowsPath(r"C:\Users\玩家\Sonnheide")
        path = root / ".build" / "ground-sky-shaders" / "dx11" / "earth_ground.fs.bin"
        name = phase1.relative_name(path, root)
        self.assertEqual(name, ".build/ground-sky-shaders/dx11/earth_ground.fs.bin")
        self.assertEqual(phase1.relative_parts(name)[0], ".build")
        with self.assertRaises(ValueError):
            phase1.relative_name(PureWindowsPath(r"D:\outside.bin"), root)

    def test_native_output_item_is_canonical_and_verifies_actual_bytes(self):
        path = self.file(".build/ground-sky-runtime/材质/albedo.png")
        item = phase1.output_item(path)
        self.assertEqual(item["path"], ".build/ground-sky-runtime/材质/albedo.png")
        self.assertTrue(phase1.validate_outputs([item]))
        path.write_bytes(b"changed")
        with self.assertRaisesRegex(RuntimeError, "bytes changed"):
            phase1.validate_outputs([item])

    def test_all_foreign_drive_ads_unc_traversal_and_alias_paths_reject(self):
        invalid = ["", "/.build/a.bin", "../a.bin", ".build/../a.bin", ".build/a/../../b.bin",
                   r".build\a.bin", r"C:\cache\a.bin", "C:cache/a.bin", "C:/cache/a.bin",
                   r"\\server\share\a.bin", "//server/share/a.bin", r"\rooted\a.bin",
                   ".build/a.bin:stream", ".build/C:/a.bin", ".build/./a.bin",
                   ".build//a.bin", ".build/a.bin/", ".build/a.bin.", ".build/a.bin ",
                   ".build/CON", ".build/NUL.bin", ".build/a?.bin", ".build/a*.bin", ".build/a|.bin", ".build/a\x00.bin", ".build/a\n.bin"]
        for name in invalid:
            with self.subTest(name=repr(name)), self.assertRaises(RuntimeError):
                phase1.generated_path(self.root, name, require_build=True)
        for name in ["assets/file.bin", ".build-other/file.bin", ".build"]:
            with self.subTest(name=name), self.assertRaises(RuntimeError):
                phase1.generated_path(self.root, name, require_build=True)

    def test_missing_output_is_not_missing_path_security_bypass(self):
        item = {"path": ".build/missing.bin", "bytes": 5, "sha256": "0" * 64}
        self.assertFalse(phase1.validate_outputs([item], missing_ok=True))
        item["path"] = ".build/../../missing.bin"
        with self.assertRaises(RuntimeError):
            phase1.validate_outputs([item], missing_ok=True)

    def test_file_and_parent_symlinks_to_inside_and_outside_reject(self):
        inside = self.file(".build/actual.bin")
        outside = self.file("elsewhere/outside.bin")
        for target in (inside, outside):
            link = self.root / ".build/link.bin"
            link.symlink_to(target)
            with self.assertRaisesRegex(RuntimeError, "Symlink"):
                phase1.validate_outputs([{"path": ".build/link.bin", "bytes": target.stat().st_size,
                                          "sha256": phase1.digest(target)}])
            link.unlink()
        parent = self.root / ".build/linked-directory"
        parent.symlink_to(inside.parent, target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, "Symlink"):
            phase1.generated_path(self.root, ".build/linked-directory/actual.bin", require_build=True)
        with self.assertRaisesRegex(RuntimeError, "Symlink"):
            phase1.generated_path(self.root, ".build/linked-directory/not-yet-created.bin", require_build=True)
        parent.unlink()

    def runtime_fixture(self, output_name):
        output = self.file(".build/ground-sky-runtime/material/albedo.png")
        manifest = {"schema_version": 1, "material_ids": [{}] * 8,
                    "sky_ids": [{"id": "light"}, {"id": "dark"}],
                    "source_lock_sha256": phase1.digest(phase1.ground.MANIFEST),
                    "outputs": [{"path": output_name, "sha256": phase1.digest(output)}]}
        self.file(".build/ground-sky-runtime/runtime.json", json.dumps(manifest).encode())
        self.file(".build/ground-sky-runtime/earth_resource_lock.hpp")
        return manifest

    def test_runtime_output_receipt_rejects_traversal_and_symlink(self):
        for name in ("../outside.bin", "material/../../outside.bin", r"material\albedo.png", "material/a.png:stream"):
            self.runtime_fixture(name)
            with self.subTest(name=name), self.assertRaises(RuntimeError):
                phase1.prepared_outputs("metal")
        self.runtime_fixture("material/albedo.png")
        output = self.runtime / "material/albedo.png"
        data = output.read_bytes()
        output.unlink()
        actual = self.file(".build/ground-sky-runtime/actual.png", data)
        output.symlink_to(actual)
        with self.assertRaisesRegex(RuntimeError, "Symlink"):
            phase1.prepared_outputs("metal")

    def test_runtime_manifest_symlink_rejects_before_read(self):
        manifest = self.file(".build/manifest.json", b"this is intentionally not JSON")
        (self.runtime / "runtime.json").symlink_to(manifest)
        with self.assertRaisesRegex(RuntimeError, "Symlink"):
            phase1.prepared_outputs("metal")

    def shader_fixture(self, unsafe_path=None):
        outputs = []
        for index in range(8):
            path = self.file(".build/ground-sky-shaders/metal/stage%d.bin" % index, b"shader-fixture-locked-1234")
            item = self.record(path)
            item["path"] = path.relative_to(self.shaders).as_posix()
            outputs.append(item)
        if unsafe_path is not None:
            outputs[0]["path"] = unsafe_path
        receipt = {"sources": {"source": "hash"}, "compile_ast_sha256": "compiler-function-hash", "outputs": outputs}
        self.file(".build/ground-sky-shaders/compiled_shaders.json", json.dumps(receipt).encode())
        return outputs

    def test_shader_output_receipt_rejects_paths_and_symlinks_without_compiling(self):
        with mock.patch.object(cooker, "shader_sources", return_value={"source": "hash"}), \
                mock.patch.object(cooker, "function_ast_hash", return_value="compiler-function-hash"), \
                mock.patch.object(cooker, "admitted_shaders") as admitted:
            self.shader_fixture()
            self.assertTrue(phase1.admitted_shader_cache("metal"))
            admitted.assert_called_once()
            admitted.reset_mock()
            for name in ("../outside", "metal/../../outside", r"metal\stage0.bin", "metal/stage0.bin:stream", "C:/drive.bin"):
                self.shader_fixture(name)
                with self.subTest(name=name), self.assertRaises(RuntimeError):
                    phase1.admitted_shader_cache("metal")
            self.shader_fixture()
            shader = self.shaders / "metal/stage0.bin"
            content = shader.read_bytes()
            shader.unlink()
            target = self.file(".build/real-shader.bin", content)
            shader.symlink_to(target)
            with self.assertRaisesRegex(RuntimeError, "Symlink"):
                phase1.admitted_shader_cache("metal")
            admitted.assert_not_called()

    def test_admission_symlink_rejects_before_source_or_network_access(self):
        outside = self.file(".build/invalid-admission.json", b"not JSON")
        phase1.ADMISSION.parent.mkdir(parents=True)
        phase1.ADMISSION.symlink_to(outside)
        with mock.patch.object(phase1, "backend", return_value="metal"), \
                mock.patch.object(phase1.geography, "validate_lock") as geography_access, \
                mock.patch.object(phase1.subprocess, "run") as process:
            with self.assertRaisesRegex(RuntimeError, "Symlink"):
                phase1.prepare(offline=True)
            geography_access.assert_not_called()
            process.assert_not_called()

    def test_shader_manifest_symlink_rejects_before_read(self):
        outside = self.file(".build/manifest.json", b"not JSON")
        (self.shaders / "compiled_shaders.json").symlink_to(outside)
        with self.assertRaisesRegex(RuntimeError, "Symlink"):
            phase1.admitted_shader_cache("metal")


class ResourceIdentityTests(unittest.TestCase):
    def test_supported_python_optional_fields_keep_same_semantics(self):
        # Python 3.12 added FunctionDef.type_params; its empty value must not
        # change a portable World created with a Python 3.9 build tool.
        node = ast.parse("def sample(value):\n    return value + 0\n").body[0]
        original = cooker.canonical_ast(node)
        newer = copy.deepcopy(node)
        newer._fields = tuple(name for name in newer._fields if name != "type_params") + ("type_params",)
        newer.type_params = []
        self.assertEqual(original, cooker.canonical_ast(newer))
        changed = ast.parse("def sample(value):\n    return value + 1\n").body[0]
        self.assertNotEqual(original, cooker.canonical_ast(changed))

    def test_real_source_only_identity_is_cache_and_backend_independent(self):
        lock = json.loads(cooker.sources.MANIFEST.read_text(encoding="utf-8"))
        with mock.patch.object(cooker, "sky_cook", side_effect=AssertionError("identity must not bake")), \
                mock.patch.object(cooker, "ggx_prefilter", side_effect=AssertionError("identity must not prefilter")), \
                mock.patch.object(cooker.subprocess, "run", side_effect=AssertionError("identity must not compile")):
            cold = cooker.runtime_identity(lock)
            expected = cooker.canonical_recipe_hash(cold)
            warm = copy.deepcopy(cold)
            warm["outputs"] = [{"path": "different-backend.bin", "sha256": "1" * 64}]
            warm["recipe_hash"] = "old-cache-identity"
            self.assertEqual(expected, cooker.canonical_recipe_hash(warm))
            evidence = json.loads((PROJECT / "docs/planning/PHASE1_EVIDENCE.json").read_text(encoding="utf-8"))
            self.assertEqual(expected, evidence["recipe_hash"])
            self.assertTrue(all("\\" not in key for key in cold["presentation"]["sources"]))

    def test_physical_material_change_invalidates_world_identity(self):
        lock = json.loads(cooker.sources.MANIFEST.read_text(encoding="utf-8"))
        original = cooker.canonical_recipe_hash(cooker.runtime_identity(lock))
        changed = copy.deepcopy(lock)
        ground = next(asset for asset in changed["assets"] if asset["kind"] == "ground")
        ground["physical_size_m"] *= 2
        self.assertNotEqual(original, cooker.canonical_recipe_hash(cooker.runtime_identity(changed)))

    def test_fingerprint_retains_zero_false_and_bytes_literals(self):
        self.assertNotEqual(cooker.canonical_ast(ast.Constant(value=0)), cooker.canonical_ast(ast.Constant(value=1)))
        self.assertNotEqual(cooker.canonical_ast(ast.Constant(value=False)), cooker.canonical_ast(ast.Constant(value=True)))
        self.assertNotEqual(cooker.canonical_ast(ast.Constant(value=b"\x00")), cooker.canonical_ast(ast.Constant(value=b"\x01")))


if __name__ == "__main__":
    unittest.main(verbosity=2)
