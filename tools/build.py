#!/usr/bin/env python3
"""Single native batch: prepare → configure → build ALL → complete CTest (optionally instrumented)."""
import argparse
import hashlib
import uuid
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import prepare_sources
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'.build'

def call(args,log=None):
    print(' '.join(str(x) for x in args),flush=True)
    result=subprocess.run([str(x) for x in args],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT if log else None)
    if result.returncode: raise SystemExit(result.returncode)

def copy_changed(src,dst):
    dst.parent.mkdir(parents=True,exist_ok=True)
    if not dst.exists() or src.read_bytes()!=dst.read_bytes():shutil.copy2(src,dst)

def runtime():
    prepare_sources.prepare()
    for source,target in [(ROOT/'assets/ui',BUILD/'runtime/ui'),(ROOT/'assets/generated',BUILD/'runtime/generated'),(ROOT/'data/contracts',BUILD/'runtime/data/contracts'),(ROOT/'data/content',BUILD/'runtime/data/content')]:
        for p in source.rglob('*'):
            if p.is_file():copy_changed(p,target/p.relative_to(source))
    for historical in ['branding','paintings']:
        shutil.rmtree(BUILD/'runtime'/historical,ignore_errors=True)
    # Keep runtime generated assets exact; removed historical Age variants must
    # not silently survive in a new admitted presentation package.
    generated=BUILD/'runtime/generated'
    for p in generated.rglob('*'):
        if p.is_file() and not (ROOT/'assets/generated'/p.relative_to(generated)).is_file():p.unlink()
    shutil.rmtree(BUILD/'runtime/menu',ignore_errors=True)
    for p in (ROOT/'assets/shaders').glob('*'):
        if p.is_file():copy_changed(p,BUILD/'runtime/shader-source'/p.name)
    copy_changed(ROOT/'src/client/client.cpp',BUILD/'runtime/recipes/pixel_surfaces.cpp')
    # Immutable presentation pack is separate from authoritative definition hash.
    asset_files=[]
    for directory in ['ui','generated','fonts','shader-source','recipes']:
        asset_files.extend(p for p in (BUILD/'runtime'/directory).rglob('*') if p.is_file() and not p.name.endswith('.admitted.json'))
    asset_manifest={'format':'SonnAssets1','files':[{'path':p.relative_to(BUILD/'runtime').as_posix(),'sha256':prepare_sources.digest(p)} for p in sorted(asset_files,key=lambda x:x.relative_to(BUILD/'runtime').as_posix())]}
    canonical_assets=json.dumps(asset_manifest,sort_keys=True,separators=(',',':'))
    (BUILD/'runtime/assets.json').write_text(canonical_assets+'\n', encoding="utf-8")
    definition_manifest=json.loads((BUILD/'runtime/definitions.json').read_text(encoding="utf-8"))
    canonical_definitions=json.dumps(definition_manifest,sort_keys=True,separators=(',',':'))
    admitted='set(SONN_DEFINITION_SHA "'+hashlib.sha256(canonical_definitions.encode()).hexdigest()+'")\nset(SONN_ASSET_SHA "'+hashlib.sha256(canonical_assets.encode()).hexdigest()+'")\n'
    metadata=BUILD/'runtime/admitted_hashes.cmake'
    if not metadata.exists() or metadata.read_text(encoding="utf-8")!=admitted:metadata.write_text(admitted, encoding="utf-8")
    # Historical paintings/branding remain in source control, outside the new arcade runtime.

def bundle():
    package=BUILD/'package/Sonnheide'
    if package.exists():shutil.rmtree(package)
    package.mkdir(parents=True)
    executable=BUILD/'native'/('RelWithDebInfo/sonnheide.exe' if os.name=='nt' else 'sonnheide')
    shutil.copy2(executable,package/executable.name)
    shutil.copytree(BUILD/'runtime',package/'runtime',ignore=shutil.ignore_patterns('*.admitted.json','admitted_hashes.cmake'))
    shutil.copy2(ROOT/'THIRD_PARTY_NOTICES.md',package/'THIRD_PARTY_NOTICES.md')
    shutil.copy2(ROOT/'OWNERSHIP.md',package/'OWNERSHIP.md')
    shutil.copytree(ROOT/'third_party/native',package/'notices/third_party/native')
    shutil.copytree(ROOT/'third_party/unicode',package/'notices/third_party/unicode')
    shutil.copytree(ROOT/'data/geo/sources/gshhg',package/'notices/gshhg')
    shutil.copytree(ROOT/'assets/source/ui/fonts/fusion-pixel',package/'notices/fusion-pixel',ignore=shutil.ignore_patterns('*.gz'))
    print('Self-contained native package:',package,flush=True)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sanitize',action='store_true',help='Instrument core/app and run the complete suite once')
    ap.add_argument('--jobs',type=int,default=min(8,os.cpu_count() or 2))
    ap.add_argument('--native-smoke',action='store_true',help='Run real native GPU flow, requires interactive desktop')
    ap.add_argument('--bundle',action='store_true',help='Package the executable and admitted resources after acceptance')
    a=ap.parse_args()
    runtime()
    cmake=shutil.which('cmake')
    if not cmake and sys.platform=='darwin':cmake=str(BUILD/'toolchain/cmake/CMake.app/Contents/bin/cmake')
    if not cmake:raise SystemExit('Install CMake3.20+ on Windows, then rerun the same entry point.')
    ctest=Path(cmake).with_name('ctest')
    modes=['native']
    for mode in modes:
        dest=BUILD/mode;dest.mkdir(parents=True,exist_ok=True)
        call([cmake,'-S',ROOT,'-B',dest,'-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DSONN_SANITIZERS='+('ON' if a.sanitize else 'OFF')])
        # The all target includes application, complete tests, dependencies and shaders once.
        log=dest/'build.log'
        with log.open('w') as f:
            try:call([cmake,'--build',dest,'--config','RelWithDebInfo','--parallel',str(max(1,a.jobs))],f)
            except SystemExit:
                print(log.read_text(errors='replace', encoding="utf-8")[-18000:]);raise
        print('All native targets built; log:',log,flush=True)
        call([ctest,'--test-dir',dest,'-C','RelWithDebInfo','--output-on-failure'])
    if a.bundle:bundle()
    if a.native_smoke:
        exe=(BUILD/'package/Sonnheide'/('sonnheide.exe' if os.name=='nt' else 'sonnheide')) if a.bundle else BUILD/'native'/('RelWithDebInfo/sonnheide.exe' if os.name=='nt' else 'sonnheide')
        evidence=BUILD/'evidence'/('native-'+uuid.uuid4().hex[:12])
        call([exe,'--smoke-test','--saves',BUILD/'evidence'/('smoke-saves-'+uuid.uuid4().hex[:12]),'--evidence',evidence])
        report=evidence/'native-flow.json'
        if not report.is_file():raise SystemExit('Native GPU flow did not produce a completion report: '+str(evidence))
        flow=json.loads(report.read_text(encoding="utf-8"))
        if flow.get('status')!='pass' or 'exit-durable-readback' not in flow.get('actions',[]):raise SystemExit('Native flow did not prove the final exit checkpoint.')
        required=['main-menu','main-menu-motion','main-menu-frozen-a','main-menu-frozen-b','blank-preview','blank-oblique','darkness','settings-zh','settings-en','settings-de','earth-map','earth-drag-a','earth-drag-b','earth-preview','earth-world','pixel-near-full','pixel-near-mean-base','pixel-near-darkness','pixel-near-light','world-name-consequences','creation-error','loaded-world']
        if any(not (evidence/(name+'.tga')).is_file() for name in required):raise SystemExit('Native screenshot evidence is incomplete: '+str(evidence))
        camera=evidence/'camera-input.json'
        if not camera.is_file() or json.loads(camera.read_text(encoding="utf-8")).get('status')!='pass':raise SystemExit('Actual SDL camera and core pick acceptance missing.')
        selection=json.loads((evidence/'map-selection.json').read_text(encoding='utf-8'))
        if (selection.get('status')!='pass' or selection.get('source_rasters_started_during_drag')!=0 or
            selection.get('source_epoch_unchanged') is not True or selection.get('frame_local_geometry_checks')!=4 or
            selection.get('actual_motion_events',0)<4 or selection.get('actual_selection_updates',0)<5 or selection.get('capture_released') is not True):
            raise SystemExit('Continuous actual native map selection acceptance missing.')
        large=json.loads((evidence/'large-world.json').read_text(encoding='utf-8'))
        if (large.get('status')!='pass' or large.get('core_width_cells')!=4096 or large.get('core_height_cells')!=4096 or
            large.get('physical_width_mm')!=8192000 or large.get('physical_height_mm')!=8192000 or
            large.get('micro_size_um')!=31250 or large.get('zero_plane_land_pick_mm')!=0 or
            large.get('continue_roundtrip_surface_identical') is not True or
            not 0<large.get('actual_geometry_bytes',0)<=large.get('geometry_budget_bytes',0)):
            raise SystemExit('Largest actual world creation/GPU/Continue acceptance missing.')
        large_gpu=evidence/large.get('native_gpu_parameters_file','')
        if not large_gpu.is_file() or large_gpu.name!='large-world-gpu.json':raise SystemExit('Largest world native GPU parameters missing.')
        json.loads(large_gpu.read_text(encoding='utf-8'))
        motion=json.loads((evidence/'presentation-motion.json').read_text(encoding='utf-8'))
        def actual_pixels(name):
            image=(evidence/(name+'.tga')).read_bytes()
            if len(image)<18 or image[2]!=2 or image[16]!=32:raise SystemExit('Actual uncompressed GPU pixels required')
            width=int.from_bytes(image[12:14],'little');height=int.from_bytes(image[14:16],'little')
            if len(image)!=18+width*height*4:raise SystemExit('Incomplete actual GPU pixel stream')
            return image[18:]
        frozen_a=actual_pixels('main-menu-frozen-a');frozen_b=actual_pixels('main-menu-frozen-b')
        live=actual_pixels('main-menu');later=actual_pixels('main-menu-motion')
        if len(frozen_a)!=len(frozen_b):raise SystemExit('Frozen frame dimensions changed')
        # A fixed clock is also asserted in the native controller. UNORM GPU
        # readback may round the final colour by one byte between swap images;
        # reject every larger change, rather than mistaking 1/255 for animation.
        frozen_max=max(abs(a-b) for a,b in zip(frozen_a,frozen_b))
        if frozen_max>1:raise SystemExit('Reduced motion changed GPU pixels beyond one UNORM rounding step')
        changed=sum(abs(a-b)>1 for a,b in zip(live,later))
        if len(live)!=len(later) or changed<1000:raise SystemExit('Live menu did not show actual visual motion')
        motion.update({'actual_gpu_pixels_frozen_identical':frozen_a==frozen_b,'gpu_rounding_tolerance_lsb':1,'frozen_max_channel_delta':frozen_max,'live_significant_changed_channels':changed,'rgba_bytes':len(live)})
        (evidence/'presentation-motion.json').write_text(json.dumps(motion,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        (BUILD/'evidence/latest-native.json').write_text(json.dumps({'directory':str(evidence),'status':'pass'},indent=2)+'\n', encoding="utf-8")
        print('Native GPU acceptance completed:',evidence,flush=True)
if __name__=='__main__':main()
