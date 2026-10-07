#!/usr/bin/env python3
"""Compiler-only local fallback; CMake remains the production build description."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sanitizers", action="store_true")
    args = parser.parse_args()
    compiler = os.environ.get("CXX") or shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        parser.error("C++20 compiler required. On Windows use CMake + MSVC.")
    out = ROOT / ".build" / ("portable-sanitizers" if args.sanitizers else "portable")
    out.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c++20", "-O1", "-g", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-I" + str(ROOT / "engine/include")]
    if args.sanitizers:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    core = [str(p) for p in sorted((ROOT / "engine/src").glob("*.cpp")) if p.name not in ("interaction.cpp", "site.cpp")]
    for name, source in [("sonnheide_headless", ROOT / "apps/headless/main.cpp"), ("sonnheide_kernel_tests", ROOT / "tests/kernel_tests.cpp"), ("sonnheide_allocation_tests", ROOT / "tests/transaction_allocation_tests.cpp"), ("sonnheide_clothing_tests", ROOT / "tests/clothing_tests.cpp")]:
        subprocess.run([compiler, *flags, *core, str(source), "-o", str(out / name)], check=True, cwd=ROOT)
        subprocess.run([str(out / name)], check=True, cwd=ROOT)
    legacy_site = out / "sonnheide_site_tests"
    subprocess.run([compiler, *flags, str(ROOT / "engine/src/site.cpp"),
                    str(ROOT / "tests/site_tests.cpp"), "-o", str(legacy_site)], check=True, cwd=ROOT)
    subprocess.run([str(legacy_site)], check=True, cwd=ROOT)
    ocean = out / "sonnheide_ocean_fft_tests"
    subprocess.run([compiler, *flags, "-I" + str(ROOT / "presentation/include"),
                    str(ROOT / "presentation/src/ocean_fft.cpp"), str(ROOT / "tests/ocean_fft_tests.cpp"),
                    "-o", str(ocean)], check=True, cwd=ROOT)
    subprocess.run([str(ocean)], check=True, cwd=ROOT)
    interaction = out / "sonnheide_interaction_tests"
    subprocess.run([compiler, *flags, "-pthread", str(ROOT / "engine/src/interaction.cpp"),
                    str(ROOT / "tests/interaction_tests.cpp"), "-o", str(interaction)], check=True, cwd=ROOT)
    subprocess.run([str(interaction)], check=True, cwd=ROOT)
    menu = out / "sonnheide_native_menu_tests"
    subprocess.run([compiler, *flags, "-I" + str(ROOT / "client/native"),
                    str(ROOT / "client/native/menu_model.cpp"), str(ROOT / "tests/native_menu_tests.cpp"),
                    "-o", str(menu)], check=True, cwd=ROOT)
    subprocess.run([str(menu)], check=True, cwd=ROOT)
    subprocess.run([shutil.which("python3") or "python3", str(ROOT / "tools/validate_project.py")], check=True, cwd=ROOT)
    subprocess.run([shutil.which("python3") or "python3", str(ROOT / "tools/validate_native.py")], check=True, cwd=ROOT)
    node = os.environ.get("SONNHEIDE_NODE") or shutil.which("node")
    if node:
        for validator in ("validate_interaction_preview.js", "validate_world_inspector.js", "validate_menu_preview.js"):
            subprocess.run([node, str(ROOT / "tools" / validator)], check=True, cwd=ROOT)
    else:
        print("SKIP: optional Node.js interaction preview checks; set SONNHEIDE_NODE to run them")


if __name__ == "__main__":
    main()
