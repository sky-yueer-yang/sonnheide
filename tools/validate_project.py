#!/usr/bin/env python3
"""Meaningful source/content admission checks; native invariants are tested by CTest."""
import hashlib
import json
from pathlib import Path
import re
import sys
import struct
import zlib
import gzip
ROOT=Path(__file__).resolve().parents[1]
def check(test,message):
    if not test:raise ValueError(message)
def main():
    for p in ['src/core/sonnheide.hpp','src/client/client.hpp','src/ui/ui.hpp','apps/game/main.cpp','CMakeLists.txt','tools/build.py']:
        check((ROOT/p).is_file(),'Missing production entry: '+p)
    for p in (ROOT/'src/core').rglob('*'):
        if p.suffix in ('.cpp','.hpp'):
            s=p.read_text(encoding="utf-8");check(not re.search(r'#include\s*[<"](?:SDL|bgfx|RmlUi|Metal)',s),'GPU/UI dependency in authoritative core: '+str(p))
    css=(ROOT/'assets/ui/application.rcss').read_text(encoding="utf-8")
    # Validate actual native pixel geometry and admitted source files. User
    # appearance preferences belong to the current art direction, not hard
    # coded bans from superseded UI designs.
    from validate_ui_artwork import validate as validate_artwork
    validate_artwork()
    check('pixel_decorator.cpp' in (ROOT/'CMakeLists.txt').read_text(encoding='utf-8'),'Native material geometry must be linked')
    check('decorator: physical-material(' in css and 'decorator: physical-tab(' in css,'Actual native physical UI missing')
    check('Sonn Arcade' in css and 'Cinzel' not in css and 'Noto Serif' not in css,'Entire interface must use the actual arcade pixel face')
    font=json.loads((ROOT/'assets/manifests/arcade_fonts.json').read_text(encoding="utf-8"))
    packed=(ROOT/font['project_path']).read_bytes();check(hashlib.sha256(packed).hexdigest()==font['compressed_sha256'],'Pixel font packed source hash')
    original=gzip.decompress(packed);check(len(original)==font['original_bytes'] and hashlib.sha256(original).hexdigest()==font['original_sha256'],'Pixel font original source hash')
    for notice in font['licenses']:check(hashlib.sha256((ROOT/notice['project_path']).read_bytes()).hexdigest()==notice['sha256'],'Pixel font notice hash')
    # Query the actual sfnt Unicode cmap, not a list of expected glyph names.
    count=struct.unpack_from('>H',original,4)[0];tables={original[12+i*16:16+i*16]:struct.unpack_from('>II',original,20+i*16) for i in range(count)}
    cmap_start,cmap_size=tables[b'cmap'];cmap=original[cmap_start:cmap_start+cmap_size]
    subtables=[]
    for i in range(struct.unpack_from('>H',cmap,2)[0]):
        platform,encoding,offset=struct.unpack_from('>HHI',cmap,4+i*8)
        if platform==0 or (platform==3 and encoding in (1,10)):subtables.append(offset)
    def has_glyph(cp):
        for offset in subtables:
            fmt=struct.unpack_from('>H',cmap,offset)[0]
            if fmt==12:
                for i in range(struct.unpack_from('>I',cmap,offset+12)[0]):
                    start,end,glyph=struct.unpack_from('>III',cmap,offset+16+i*12)
                    if start<=cp<=end:return glyph+cp-start!=0
            elif fmt==4 and cp<65536:
                n=struct.unpack_from('>H',cmap,offset+6)[0]//2
                for i in range(n):
                    end=struct.unpack_from('>H',cmap,offset+14+i*2)[0];start=struct.unpack_from('>H',cmap,offset+16+n*2+i*2)[0]
                    if start<=cp<=end:
                        delta=struct.unpack_from('>h',cmap,offset+16+n*4+i*2)[0];address=offset+16+n*6+i*2;shift=struct.unpack_from('>H',cmap,address)[0]
                        glyph=struct.unpack_from('>H',cmap,address+shift+(cp-start)*2)[0] if shift else cp
                        return glyph!=0 and ((glyph+delta)&65535)!=0
        return False
    required=set('SONNHEIDE ÄÖÜäöüß新世界退出顺序汉帝国 ·×–—')
    for line in (ROOT/'assets/ui/locales.tsv').read_text(encoding="utf-8").splitlines()[1:]:required.update(line.partition('\t')[2])
    missing=sorted(c for c in required if not c.isspace() and not has_glyph(ord(c)))
    check(not missing,'Missing arcade UI glyphs: '+repr(missing))
    runtime=ROOT/'.build/runtime'
    manifest=json.loads((runtime/'definitions.json').read_text(encoding="utf-8"))
    check(len(manifest['files'])==23,'Definition packs missing')
    for f in manifest['files']:
        p=runtime/f['path'];check(p.is_file(),'Missing runtime definition '+str(p));check(hashlib.sha256(p.read_bytes()).hexdigest()==f['sha256'],'Definition admission mismatch')
    for p in ['fonts/fusion-pixel-12px-proportional-zh_hans.otf','ui/application.rml','ui/locales.tsv']:
        check((runtime/p).is_file(),'Missing runtime resource '+p)
    check(not (runtime/'geo').exists(),'Removed world-map source must not enter production runtime')
    check((ROOT/'.build/test-resources/geo/gshhs_f.b').is_file(),'Historical source remains available for geometry regression')
    ui=(ROOT/'assets/ui/application.rml').read_text(encoding='utf-8')
    check(all(token not in ui for token in ['earth-mode','earth-controls','earth-selection','map-controls']),'World-map UI must be removed')
    check(not (runtime/'menu/arcade-garden-v2.png').exists(),'Superseded menu illustration must not enter active runtime')
    print('PASS source boundaries, crisp original icons, immutable definitions and runtime resources')
if __name__=='__main__':
    try:main()
    except Exception as e:print('FAIL:',e,file=sys.stderr);sys.exit(1)
