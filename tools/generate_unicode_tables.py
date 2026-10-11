#!/usr/bin/env python3
"""Reproduce the Unicode 13.0.0 NFC tables used by the integer JSON wire.
Python 3.9 standard library, no network or external dependency. The table stays
checked in; normal C++ builds do not execute this recipe. Licensing notice:
third_party/unicode/LICENSE.txt and third_party/unicode/NOTICE.md.
"""
import pathlib
import unicodedata as u

if u.unidata_version != '13.0.0':
    raise SystemExit('Requires Unicode 13.0.0 database; refusing a silent wire-profile change')
target = pathlib.Path(__file__).resolve().parents[1] / 'src' / 'core' / 'unicode_tables.inc'
with target.open('w', encoding='utf-8', newline='\n') as f:
    f.write('// Generated from Python Unicode '+u.unidata_version+' canonical normalization data; no runtime Python dependency.\n')
    f.write('struct DecompRow { uint32_t cp,a,b; };\nstatic constexpr DecompRow decompositions[]={\n')
    comp = []
    for cp in range(0x110000):
        d = u.decomposition(chr(cp))
        if d and not d.startswith('<'):
            v = [int(x, 16) for x in d.split()]
            f.write('{%d,%d,%d},\n' % (cp, v[0], v[1] if len(v) > 1 else 0))
            if len(v) == 2 and u.normalize('NFC', ''.join(map(chr, v))) == chr(cp):
                comp.append((v[0], v[1], cp))
    f.write('};\nstruct CccRow { uint32_t cp; uint8_t ccc; };\nstatic constexpr CccRow combining_classes[]={\n')
    for cp in range(0x110000):
        c = u.combining(chr(cp))
        if c:
            f.write('{%d,%d},\n' % (cp, c))
    f.write('};\nstruct ComposeRow { uint32_t a,b,cp; };\nstatic constexpr ComposeRow compositions[]={\n')
    for x in sorted(comp):
        f.write('{%d,%d,%d},\n' % x)
    f.write('};\n')
