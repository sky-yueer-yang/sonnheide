#!/usr/bin/env python3
"""Cook SHA-locked Poly Haven sources for the native ground renderer (stdlib, 3.9+).

Original 2K PBR PNG bytes are retained. 8K Pure Sky is decoded to linear RGBA16F
with bounded hemisphere processing, fixed night calibration, full mip chain and
Lambertian spherical-harmonic irradiance. No runtime network or simulation RNG.
"""
import argparse
import ast
import array
import hashlib
import json
import math
import platform
import subprocess
from pathlib import Path
import struct
import zipfile
import prepare_ground_sources as sources
ROOT = Path(__file__).resolve().parents[1]
RECIPE = 'sonnheide-ground-sky-v2-rgba16f-sh9-ggx-fixed-age'
RENDERER_ALGORITHM_VERSION = 'earth-pbr-hex-ggx-orthographic-ocean-v3'
AST_FINGERPRINT_FORMAT = 'sonnheide-python-ast-json-v1'
# Exact pre-split implementation identities; legacy migration requires both.
LEGACY_TOOL_SHA256 = '86a5783c4aee4f7bdfcc15dfb1b36795e3a9fae50bcf409675604d6a58a8cdd4'
LEGACY_IMAGE_AST_SHA256 = '8f56c74f368a990dede5f9974903b11e7b1441fe08b47c1e69eb6bfbd6f10109'
# The exact unchanged image functions under Python 3.9, 3.12 and 3.14 ast.dump.
# Migration still requires their new stable identity AND both actual output SHA.
LEGACY_IMAGE_DUMP_HASHES = (
    LEGACY_IMAGE_AST_SHA256,
    'f84e727064af85c2dad0fbb3f1210d5aad8ba7f91142032e7aa9c968c615b5fa',
    '6a489c4a0aed78b6782c92abb97c6abab8d009aaae939e94c89280c26308800d',
)
UNCHANGED_IMAGE_CANONICAL_SHA256 = 'ac3ddb3d6d649d141cb4e9614f598c5ec442c7d9f4ca72d29cea25e47eccafa7'


def radiance_rows(path):
    with path.open('rb') as source:
        signature = source.readline()
        if signature not in (b'#?RADIANCE\n', b'#?RGBE\n'):
            raise ValueError('Invalid Radiance signature')
        while source.readline().strip():
            pass
        axis = source.readline().split()
        if axis[0] != b'-Y' or axis[2] != b'+X':
            raise ValueError('Unsupported HDR orientation')
        height, width = int(axis[1]), int(axis[3])
        for y in range(height):
            header = source.read(4)
            if header != bytes((2, 2, width >> 8, width & 255)):
                raise ValueError('Invalid HDR scanline')
            channels = []
            for _ in range(4):
                out = bytearray()
                while len(out) < width:
                    count = source.read(1)
                    if not count or count[0] == 0:
                        raise ValueError('Truncated HDR RLE')
                    n = count[0]
                    if n > 128:
                        value = source.read(1)
                        if len(value) != 1:
                            raise ValueError('Truncated HDR run')
                        out.extend(value * (n - 128))
                    else:
                        out.extend(source.read(n))
                    if len(out) > width:
                        raise ValueError('HDR run crosses scanline')
                channels.append(out)
            yield y, width, height, channels


def sky_cook(path, output, darkness):
    # RGBE->float scale is exact exponent conversion; all inputs must be finite.
    exponents = [0.0] + [math.ldexp(1.0, e - 136) for e in range(1, 256)]
    # Photographic HDR capture exposures are unrelated. Explicit night calibration,
    # applied identically to visible sky, SH irradiance, reflections and water.
    calibration = 0.018 if darkness else 0.65
    sh = [[0.0, 0.0, 0.0] for _ in range(9)]
    total_weight = 0.0
    base = array.array('f')
    width = height = 0
    row_luminance = []
    for y, width, height, channels in radiance_rows(path):
        row = array.array('f')
        r,g,b,e = channels
        for x in range(width):
            scale = exponents[e[x]] * calibration
            row.extend((min(r[x]*scale,65504.0), min(g[x]*scale,65504.0), min(b[x]*scale,65504.0),1.0))
        base.extend(row)
        if y % 128 == 0:
            print('sky',path.name,y,'/',height,flush=True)
    # Replace lower hemisphere with mirrored upper sky radiance. No photographic
    # ground or artificial bright strip survives. Fade to a neutral horizon tint.
    for y in range(height//2,height):
        mirror=height-1-y
        offset=y*width*4
        above=mirror*width*4
        elevation=(y-height*.5)/(height*.5)
        blend=min(1.0,elevation*2.0)
        for x in range(width):
            a=offset+x*4
            s=above+x*4
            for c in range(3):
                base[a+c]=base[s+c]*(1.0-blend)+base[s+c]*0.28*blend
    # Integrate SH at equal equirectangular strides with spherical area weights.
    step=16
    for y in range(0,height,step):
        theta=math.pi*(y+.5)/height
        sy=math.cos(theta); sr=math.sin(theta)
        for x in range(0,width,step):
            phi=2*math.pi*((x+.5)/width-.5)
            sx=sr*math.cos(phi);sz=sr*math.sin(phi)
            basis=(.282095,.488603*sy,.488603*sz,.488603*sx,1.092548*sx*sy,
                   1.092548*sy*sz,.315392*(3*sz*sz-1),1.092548*sx*sz,.546274*(sx*sx-sy*sy))
            a=(y*width+x)*4
            for k in range(9):
                for c in range(3):
                    sh[k][c]+=base[a+c]*basis[k]*sr
            total_weight+=sr
    # Irradiance / PI: convolution l0 PI, l1 2PI/3, l2 PI/4, then SH basis.
    for k in range(9):
        factor=(1.0 if k==0 else 2/3 if k<4 else .25)*4*math.pi/total_weight
        sh[k]=[v*factor*(.282095 if k==0 else .488603 if k<4 else
               1.092548 if k in (4,5,7) else .315392 if k==6 else .546274) for v in sh[k]]
    levels=[]
    w,h=width,height
    with output.open('wb') as result:
        result.write(b'SNHDR01\0')
        result.write(struct.pack('<III',w,h,1+int(math.log2(max(w,h)))))
        result.write(struct.pack('<27f',*(v for coefficient in sh for v in coefficient)))
        while True:
            levels.append([w,h])
            # Half packing in rows keeps transient bytes bounded.
            for y in range(h):
                result.write(struct.pack('<'+'e'*(w*4),*base[y*w*4:(y+1)*w*4]))
            if w==1 and h==1:
                break
            nw,nh=max(1,w//2),max(1,h//2)
            small=array.array('f')
            for y in range(nh):
                for x in range(nw):
                    offsets=[(min(h-1,y*2+dy)*w+min(w-1,x*2+dx))*4 for dy in range(2) for dx in range(2)]
                    small.extend(sum(base[a+c] for a in offsets)*.25 for c in range(4))
            base=small;w,h=nw,nh
    return {'levels':levels,'calibration':calibration,'lower_hemisphere':'mirrored-upper-radiance-fade-0.28',
            'diffuse':'spherical-area-weighted-Lambert-SH9','specular':'linear-radiance-area-mips-roughness-approximation'}


def ggx_prefilter(source_path, output):
    # Standard split-sum IBL: V=N, GGX Hammersley importance samples weighted N.L.
    # PDF-derived source mip suppresses tiny HDR sun aliasing without clipping light.
    source_levels=[]
    with source_path.open('rb') as stream:
        header=stream.read(128)
        w,h,count=struct.unpack('<III',header[8:20])
        for level in range(count):
            amount=w*h*8
            if level>=2:
                raw=stream.read(amount)
                floats=array.array('f')
                for pixel in struct.iter_unpack('<4e',raw):
                    floats.extend(pixel)
                source_levels.append((w,h,floats))
            else:
                stream.seek(amount,1)
            w,h=max(1,w//2),max(1,h//2)
    def sample(direction,lod):
        sx,sy,sz=direction
        u=(math.atan2(sz,sx)/(2*math.pi)+.5)%1
        v=math.acos(max(-1,min(1,sy)))/math.pi
        level=min(len(source_levels)-1,max(0,int(lod)-2))
        w,h,pixels=source_levels[level]
        x=u*w-.5;y=v*h-.5
        ix=math.floor(x);iy=math.floor(y);fx=x-ix;fy=y-iy
        # Seam-safe longitude, clamped latitude; bilinear in linear radiance.
        a=(max(0,min(h-1,iy))*w+(ix%w))*4
        b=(max(0,min(h-1,iy))*w+((ix+1)%w))*4
        c=(max(0,min(h-1,iy+1))*w+(ix%w))*4
        d=(max(0,min(h-1,iy+1))*w+((ix+1)%w))*4
        return tuple((pixels[a+k]*(1-fx)+pixels[b+k]*fx)*(1-fy)+
                     (pixels[c+k]*(1-fx)+pixels[d+k]*fx)*fy for k in range(3))
    def radical_inverse(bits):
        result=0.;scale=.5
        while bits:
            result+=(bits&1)*scale;bits>>=1;scale*=.5
        return result
    samples=128; mips=10; w,h=512,256
    with output.open('wb') as result:
        result.write(b'SNIBL01\0'+struct.pack('<III',w,h,mips)+bytes(108))
        for level in range(mips):
            rough=level/(mips-1);alpha=max(.0001,rough*rough);a2=alpha*alpha
            directions=[]
            if level:
                for i in range(samples):
                    phi=2*math.pi*i/samples;xi=radical_inverse(i)
                    ct=math.sqrt((1-xi)/(1+(a2-1)*xi));st=math.sqrt(max(0,1-ct*ct))
                    lx=2*ct*st*math.cos(phi);ly=2*ct*st*math.sin(phi);lz=2*ct*ct-1
                    pdf=a2/(4*math.pi*((ct*ct)*(a2-1)+1)**2)
                    directions.append((lx,ly,lz,pdf))
            for y in range(h):
                row=[];theta=math.pi*(y+.5)/h;ny=math.cos(theta);nr=math.sin(theta)
                for x in range(w):
                    phi=2*math.pi*((x+.5)/w-.5);nx=nr*math.cos(phi);nz=nr*math.sin(phi)
                    if not level:
                        color=sample((nx,ny,nz),4)
                    else:
                        # tangent = normalized cross(up,N), stable at polar pixels.
                        if abs(ny)<.999:
                            tx,tz=nz/nr,-nx/nr;ty=0.
                        else:
                            length=math.hypot(ny,nz);tx,ty,tz=0.,-nz/length,ny/length
                        bx=ny*tz-nz*ty;by=nz*tx-nx*tz;bz=nx*ty-ny*tx
                        total=[0.,0.,0.];weight=0.
                        for lx,ly,lz,pdf in directions:
                            if lz<=0: continue
                            L=(tx*lx+bx*ly+nx*lz,ty*lx+by*ly+ny*lz,tz*lx+bz*ly+nz*lz)
                            solid_angle_texel=2*math.pi*math.pi*max(.001,math.sqrt(max(0.,1-L[1]*L[1])))/(8192*4096)
                            lod=max(0.,.5*math.log2(1/(samples*pdf*solid_angle_texel)))
                            rgb=sample(L,lod)
                            for c in range(3):total[c]+=rgb[c]*lz
                            weight+=lz
                        color=tuple(v/max(weight,.0001) for v in total)
                    row.extend((*color,1.0))
                result.write(struct.pack('<'+'e'*len(row),*row))
            print('GGX',source_path.parent.name,'roughness',round(rough,3),'size',w,h,flush=True)
            w,h=max(1,w//2),max(1,h//2)
    return {'samples':samples,'distribution':'GGX importance sampling V=N; Hammersley; N.L normalization',
            'source_filter':'PDF-derived solid-angle Radiance mip; seam-safe bilinear',
            'roughness_levels':10,'resolution':[512,256]}


def canonical_ast(node):
    """Own versioned AST encoding, independent of Python's debug ast.dump.

    Python 3.12 added empty type_params; 3.14 dump hides empty lists. Neither
    changes these algorithms. Absent/None/empty optional grammar fields encode
    identically, while every nonempty field, node kind and literal is retained.
    """
    if isinstance(node, ast.AST):
        return [type(node).__name__, {name: canonical_ast(value)
            for name, value in ast.iter_fields(node) if value is not None and value != []}]
    if isinstance(node, list):
        return [canonical_ast(value) for value in node]
    if isinstance(node, bytes):
        return {'bytes_hex': node.hex()}
    return node


def function_ast_hash(names):
    tree=ast.parse(Path(__file__).read_text(encoding='utf-8'))
    nodes=[node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name in names]
    if {node.name for node in nodes} != set(names):
        raise ValueError('Missing function in the fixed cook fingerprint')
    payload=json.dumps([canonical_ast(node) for node in nodes],sort_keys=True,
                       separators=(',',':'),allow_nan=False)
    return hashlib.sha256((AST_FINGERPRINT_FORMAT+'\n'+payload).encode('utf-8')).hexdigest()


def shader_sources():
    folder=ROOT/'presentation/native/shaders'
    paths=sorted(p for p in folder.iterdir() if p.suffix in ('.sc','.sh'))
    paths.append(ROOT/'data/native_dependencies.lock.json')
    return {p.relative_to(ROOT).as_posix():sources.digest(p) for p in paths}


def admitted_shaders(folder):
    receipt=json.loads((folder/'compiled_shaders.json').read_text())
    if receipt['sources']!=shader_sources() or receipt['compile_ast_sha256']!=function_ast_hash({'cook_shaders'}):
        raise ValueError('Compiled shaders do not match current source/compile recipe; run --shaders-only')
    if len(receipt['outputs'])!=8:
        raise ValueError('Expected exactly eight compiled Earth shader stages')
    for item in receipt['outputs']:
        path=folder/item['path']
        if path.is_symlink() or not path.is_file() or path.stat().st_size!=item['bytes'] or sources.digest(path)!=item['sha256']:
            raise ValueError('Compiled shader SHA/size admission failed: '+str(path))
    return receipt['outputs']


def cook_shaders(output, compiler=None):
    profile='metal' if platform.system()=='Darwin' else 's_5_0'
    platform_name='osx' if platform.system()=='Darwin' else 'windows'
    target='metal' if platform.system()=='Darwin' else 'dx11'
    if compiler is None:
        import build_native
        cmake=build_native.cmake_command(offline=True)
        build=ROOT/'.build/shader-tools'
        if platform.system()=='Windows':
            guesses=[build/'cmake/bgfx/Release/shaderc.exe',build/'cmake/bgfx/shaderc.exe']
        else:
            guesses=[build/'cmake/bgfx/shaderc']
        compiler=next((p for p in guesses if p.exists()),None)
        if compiler is None:
            native=ROOT/'.build/native-sources'
            subprocess.run([cmake,'-S',str(native/'bgfx-cmake'),'-B',str(build),
                '-DBGFX_DIR='+str(native/'bgfx'),'-DBX_DIR='+str(native/'bx'),
                '-DBIMG_DIR='+str(native/'bimg'),'-DBGFX_BUILD_TOOLS=ON',
                '-DBGFX_BUILD_EXAMPLES=OFF','-DCMAKE_BUILD_TYPE=Release'],check=True)
            subprocess.run([cmake,'--build',str(build),'--config','Release','--target','shaderc','--parallel','4'],check=True)
            compiler=next((p for p in guesses if p.exists()),None)
            if compiler is None:
                raise RuntimeError('Pinned shaderc build did not produce a compiler')
    folder=output/target
    folder.mkdir(parents=True,exist_ok=True)
    for name in ('earth_ground','earth_sky','earth_map','earth_atlas'):
        for stage,kind in (('vs','vertex'),('fs','fragment')):
            subprocess.run([str(compiler),'-f',str(ROOT/'presentation/native/shaders'/(name+'.'+stage+'.sc')),
                '-o',str(folder/(name+'.'+stage+'.bin')),'--type',kind,'--platform',platform_name,
                '-p',profile,'--varyingdef',str(ROOT/'presentation/native/shaders/earth_varying.def.sc'),
                '-i',str(ROOT/'.build/native-sources/bgfx/src')+';'+str(ROOT/'presentation/native/shaders'),'-O','3'],check=True)
    compiled=[]
    for path in sorted(folder.glob('*.bin')):
        compiled.append({'path':path.relative_to(output).as_posix(),'sha256':sources.digest(path),'bytes':path.stat().st_size})
    (output/'compiled_shaders.json').write_text(json.dumps({'sources':shader_sources(),
        'compile_ast_sha256':function_ast_hash({'cook_shaders'}),'outputs':compiled},sort_keys=True,indent=2)+'\n')
    print('PASS: pinned native Earth shaders',target)


def runtime_identity(lock):
    """Only immutable source/algorithm inputs; no cache, cooked byte or backend."""
    presentation_sources=shader_sources()
    for relative in ('presentation/native/earth_renderer.cpp','presentation/native/earth_renderer.hpp',
                     'presentation/native/earth_camera.hpp',
                     'platform/src/image_decode_bimg.cpp'):
        presentation_sources[relative]=sources.digest(ROOT/relative)
    runtime={'schema_version':1,'recipe':RECIPE,'source_lock_sha256':sources.digest(sources.MANIFEST),
             'fingerprint_format':AST_FINGERPRINT_FORMAT,
             'image_algorithm_sha256':function_ast_hash({'radiance_rows','sky_cook','ggx_prefilter'}),
             'presentation':{'algorithm_version':RENDERER_ALGORITHM_VERSION,'sources':presentation_sources,
                 'compile_ast_sha256':function_ast_hash({'cook_shaders'})},
             'material_ids':[],'sky_ids':[]}
    for entry in lock['assets']:
        if entry['kind']=='ground':
            runtime['material_ids'].append({'id':entry['id'],'physical_size_m':entry['physical_size_m'],
                'files':{f['slot']:entry['id']+'/'+f['filename'] for f in entry['files']}})
        else:
            runtime['sky_ids'].append({'id':entry['id'],'role':entry['role'],'file':entry['id']+'/radiance.bin'})
    return runtime


def canonical_recipe_hash(runtime):
    canonical={key:value for key,value in runtime.items() if key not in ('outputs','recipe_hash')}
    return hashlib.sha256(json.dumps(canonical,sort_keys=True,separators=(',',':'),
                                     allow_nan=False).encode('utf-8')).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'.build/ground-sky-runtime')
    parser.add_argument('--fetch',action='store_true')
    parser.add_argument('--shaders-only',action='store_true')
    parser.add_argument('--shaderc',type=Path)
    parser.add_argument('--shader-output',type=Path,default=ROOT/'.build/ground-sky-shaders')
    args=parser.parse_args()
    if args.shaders_only:
        cook_shaders(args.shader_output,args.shaderc)
        return
    lock,packs=sources.prepare(fetch=args.fetch)
    args.output.mkdir(parents=True,exist_ok=True)
    compiled_shaders=admitted_shaders(args.shader_output)
    runtime=runtime_identity(lock)
    runtime['outputs']=[]
    for entry,pack in zip(lock['assets'],packs):
        folder=args.output/entry['id'];folder.mkdir(exist_ok=True)
        with zipfile.ZipFile(pack) as archive:
            for item in entry['files']:
                destination=folder/item['filename']
                if not destination.exists() or sources.digest(destination)!=item['sha256']:
                    destination.write_bytes(archive.read(item['filename']))
                sources.verify_image(destination,item)
                if entry['kind']=='ground':
                    runtime['outputs'].append({'path':destination.relative_to(args.output).as_posix(),'sha256':item['sha256']})
        if entry['kind']!='ground':
            output=folder/'radiance.bin';receipt=folder/'recipe.json'
            image_ast=function_ast_hash({'radiance_rows','sky_cook','ggx_prefilter'})
            key=hashlib.sha256((RECIPE+entry['files'][0]['sha256']+image_ast).encode()).hexdigest()
            specular=folder/'specular.bin'
            previous=json.loads(receipt.read_text()) if receipt.exists() else {}
            legacy_keys={hashlib.sha256((RECIPE+entry['files'][0]['sha256']+old).encode()).hexdigest()
                         for old in (*LEGACY_IMAGE_DUMP_HASHES,LEGACY_TOOL_SHA256)}
            compatible_legacy=(image_ast==UNCHANGED_IMAGE_CANONICAL_SHA256 and previous.get('key') in legacy_keys)
            valid=(output.exists() and specular.exists() and (previous.get('key')==key or compatible_legacy)
                and previous.get('radiance_sha256')==sources.digest(output)
                and previous.get('specular_sha256')==sources.digest(specular))
            if not valid:
                cooked=sky_cook(folder/entry['files'][0]['filename'],output,entry['role']=='age_of_darkness')
                ggx=ggx_prefilter(output,specular)
                cooked['specular']=ggx
                receipt.write_text(json.dumps({'key':key,'image_algorithm_sha256':image_ast,
                    'fingerprint_format':AST_FINGERPRINT_FORMAT,'radiance_sha256':sources.digest(output),
                    'specular_sha256':sources.digest(specular),**cooked},sort_keys=True,indent=2)+'\n')
            if valid and compatible_legacy:
                previous['migrated_legacy_key']=previous.get('migrated_legacy_key',previous['key'])
                previous['key']=key
                previous['image_algorithm_sha256']=image_ast
                previous['fingerprint_format']=AST_FINGERPRINT_FORMAT
                receipt.write_text(json.dumps(previous,sort_keys=True,indent=2)+'\n')
                print('PASS: unchanged image algorithm + exact legacy key + both file SHA; migrated image cache',entry['id'])
            runtime['outputs'].append({'path':output.relative_to(args.output).as_posix(),'sha256':sources.digest(output)})
            runtime['outputs'].append({'path':specular.relative_to(args.output).as_posix(),'sha256':sources.digest(specular)})
            # Runtime bundle only needs cooked binary, source remains immutable Release pack.
            (folder/entry['files'][0]['filename']).unlink()
    # World presentation identity names original inputs and deterministic recipes.
    # Per-build image bytes and backend binaries are separately SHA-admitted in the
    # compiled header; libm last-bit or Metal/D3D compilation differences cannot
    # silently change source identity or make the same World non-portable.
    runtime['recipe_hash']=canonical_recipe_hash(runtime)
    (args.output/'runtime.json').write_text(json.dumps(runtime,sort_keys=True,indent=2)+'\n')
    records=[{'path':item['path'],'sha256':item['sha256'],
              'bytes':(args.output/item['path']).stat().st_size} for item in runtime['outputs']]
    records.append({'path':'runtime.json','sha256':sources.digest(args.output/'runtime.json'),
                    'bytes':(args.output/'runtime.json').stat().st_size})
    header=['// Generated from SHA-admitted sources and fixed cook recipe. Do not edit.',
            '#pragma once','#include <array>','#include <cstdint>',
            'namespace sonnheide::native::earth_resource_lock {',
            'struct File { const char* path; const char* sha256; std::uint64_t bytes; };',
            'inline constexpr char recipe_hash[] = "'+runtime['recipe_hash']+'";',
            'inline constexpr std::array<File,'+str(len(records))+'> files = {{']
    for item in records:
        header.append('{"'+item['path']+'","'+item['sha256']+'",'+str(item['bytes'])+'ULL},')
    header += ['}};','inline constexpr std::array<File,'+str(len(compiled_shaders))+'> shader_files = {{']
    for item in compiled_shaders:
        header.append('{\"'+item['path']+'\",\"'+item['sha256']+'\",'+str(item['bytes'])+'ULL},')
    header += ['}};','}']
    (args.output/'earth_resource_lock.hpp').write_text('\n'.join(header)+'\n')

    print('PASS: native 8 PBR + 2 full-resolution HDR resources',runtime['recipe_hash'])


if __name__=='__main__':
    main()
