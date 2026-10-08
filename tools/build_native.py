#!/usr/bin/env python3
"""Build the native Windows Direct3D11 or macOS Metal client without modifying global tooling. Python 3.9+ stdlib."""
import argparse
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import vendor_native
import prepare_phase1

ROOT = Path(__file__).resolve().parents[1]


def cmake_command(offline=False):
    external = shutil.which('cmake')
    if external:
        return external
    if platform.system() != 'Darwin':
        raise RuntimeError('CMake >= 3.24 must be available on PATH for Windows builds; CI uses its preinstalled official CMake')
    lock = json.loads((ROOT / 'data/native_dependencies.lock.json').read_text())
    entry = next(item for item in lock['build_tools'] if item['name'] == 'cmake')
    target = ROOT / '.build/native-tools/cmake-3.31.10-macos-universal/CMake.app/Contents/bin/cmake'
    if not target.exists():
        archive = vendor_native.archive_for(entry, offline=offline)
        folder = ROOT / '.build/native-tools'
        folder.mkdir(parents=True, exist_ok=True)
        with tarfile.open(archive) as tar:
            for item in tar.getmembers():
                p = Path(item.name)
                if p.is_absolute() or '..' in p.parts or item.isdev():
                    raise RuntimeError('Unsafe CMake archive')
            tar.extractall(folder)
    return str(target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--allow-upstream', action='store_true')
    parser.add_argument('--run', action='store_true')
    parser.add_argument('--smoke-test', action='store_true')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--config', default='RelWithDebInfo')
    args = parser.parse_args()
    vendor_args = [sys.executable, str(ROOT / 'tools/vendor_native.py')]
    if args.offline:
        vendor_args.append('--offline')
    if args.allow_upstream:
        vendor_args.append('--allow-upstream')
    subprocess.run(vendor_args, check=True, cwd=ROOT)
    # Every native build consumes real, complete and independently verified
    # terrain/sky resources. Warm-cache admission never repeats HDR cooking.
    prepare_phase1.prepare(args.offline, args.allow_upstream)
    cmake = cmake_command(args.offline)
    build = ROOT / '.build/native'
    configure = [cmake, '-S', str(ROOT), '-B', str(build), '-DSONNHEIDE_BUILD_NATIVE=ON',
                 '-DBUILD_TESTING=ON', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5', '-DCMAKE_BUILD_TYPE=' + args.config]
    if platform.system() == 'Darwin':
        configure += ['-G', 'Unix Makefiles', '-DCMAKE_OSX_ARCHITECTURES=' + platform.machine()]
    elif platform.system() != 'Windows':
        raise RuntimeError('The native client currently targets Windows and macOS')
    subprocess.run(configure, check=True)
    # Complete the batch first, then build and verify the entire project once.
    subprocess.run([cmake, '--build', str(build), '--config', args.config,
                    '--parallel', str(max(1, args.jobs))], check=True)
    ctest = shutil.which('ctest') or str(Path(cmake).with_name('ctest.exe' if platform.system() == 'Windows' else 'ctest'))
    subprocess.run([ctest, '--test-dir', str(build), '-C', args.config, '--output-on-failure'], check=True)
    if args.run or args.smoke_test:
        executable = build / 'Sonnheide.app/Contents/MacOS/Sonnheide'
        if platform.system() == 'Windows':
            executable = build / args.config / 'Sonnheide.exe'
            if not executable.exists():
                executable = build / 'Sonnheide.exe'
        subprocess.run([str(executable)] + (['--smoke-test'] if args.smoke_test else []), check=True)


if __name__ == '__main__':
    main()
