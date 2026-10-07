#!/usr/bin/env python3
"""Offline Noto font byte, SFNT and native-menu coverage audit; Python 3.9+ stdlib."""
import hashlib
import json
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[5]
EXPECTED_SHA256 = '2a2eae2628df83556c54018c41e20fa532c1b862c5256ae8b3f23feb918d12ca'
EXPECTED_BYTES = 24543080


def require(condition, message):
    if not condition:
        raise ValueError(message)


def inspect_font(path, characters):
    data = path.read_bytes()
    require(len(data) == EXPECTED_BYTES, 'Font length differs from original')
    require(hashlib.sha256(data).hexdigest() == EXPECTED_SHA256, 'Font SHA-256 differs from original')
    require(data[:4] == b'OTTO', 'Expected an OpenType CFF font')
    count = struct.unpack_from('>H', data, 4)[0]
    require(12 + count * 16 <= len(data), 'Truncated SFNT directory')
    tables = {}
    for index in range(count):
        tag, checksum, offset, length = struct.unpack_from('>4sIII', data, 12 + index * 16)
        require(tag not in tables, 'Duplicate table')
        require(offset % 4 == 0 and offset + length <= len(data), 'Invalid table bounds')
        tables[tag] = (offset, length)
    require(b'CFF ' in tables and b'fvar' not in tables, 'Expected static CFF outlines')
    for tag in (b'cmap', b'maxp', b'name'):
        require(tag in tables, 'Missing required table: ' + repr(tag))
    maximum, length = tables[b'maxp']
    require(length >= 6, 'Truncated maxp table')
    glyphs = struct.unpack_from('>H', data, maximum + 4)[0]
    require(glyphs > 1, 'No glyphs')

    names = {}
    offset, length = tables[b'name']
    table = data[offset:offset + length]
    require(length >= 6, 'Truncated name table')
    _, records, strings = struct.unpack_from('>HHH', table)
    require(6 + records * 12 <= length, 'Truncated name records')
    for index in range(records):
        platform, encoding, language, name_id, size, start = struct.unpack_from('>HHHHHH', table, 6 + index * 12)
        require(strings + start + size <= length, 'Invalid name bounds')
        raw = table[strings + start:strings + start + size]
        if platform in (0, 3):
            value = raw.decode('utf-16-be')
            names.setdefault(name_id, set()).add(value)
    require('Noto Serif CJK SC' in names.get(1, set()), 'Unexpected font family')
    require(any(value.startswith('Version 2.003;') for value in names.get(5, set())), 'Unexpected font version')
    require(any('SIL Open Font License' in value for value in names.get(13, set())), 'Missing embedded OFL declaration')

    offset, length = tables[b'cmap']
    table = data[offset:offset + length]
    require(length >= 4, 'Truncated cmap')
    _, records = struct.unpack_from('>HH', table)
    require(4 + records * 8 <= length, 'Truncated cmap records')
    choices = []
    for index in range(records):
        platform, encoding, start = struct.unpack_from('>HHI', table, 4 + index * 8)
        require(start + 2 <= length, 'Invalid cmap offset')
        form = struct.unpack_from('>H', table, start)[0]
        if form == 12 and (platform == 0 or (platform == 3 and encoding == 10)):
            choices.append(start)
    require(choices, 'Missing full Unicode cmap format 12')
    start = choices[0]
    require(start + 16 <= length, 'Truncated cmap format 12')
    form, reserved, size, language, count = struct.unpack_from('>HHIII', table, start)
    require(size == 16 + count * 12 and start + size <= length, 'Invalid cmap format 12 size')
    groups = []
    previous_end = -1
    for index in range(count):
        first, last, glyph = struct.unpack_from('>III', table, start + 16 + index * 12)
        require(previous_end < first <= last <= 0x10ffff, 'Unsorted or invalid cmap Unicode ranges')
        require(glyph + last - first < glyphs, 'cmap glyph outside maxp glyph range')
        groups.append((first, last, glyph))
        previous_end = last
    missing = []
    for character in sorted(set(characters)):
        code = ord(character)
        present = any(first <= code <= last and glyph + code - first > 0 for first, last, glyph in groups)
        if not present:
            missing.append('U+%04X %s' % (code, character))
    require(not missing, 'Missing menu glyphs: ' + ', '.join(missing))
    return {
        'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data),
        'sfnt': 'OTTO', 'table_count': len(tables), 'outline': 'CFF', 'variable': False,
        'num_glyphs': glyphs, 'cmap_format': 12, 'cmap_groups': len(groups),
        'tested_unique_characters': len(set(characters)), 'missing_characters': [],
        'names': {str(key): sorted(values) for key, values in sorted(names.items())},
    }


def menu_characters():
    characters = set(chr(code) for code in range(32, 127))
    for relative in ('client/native/menu_model.cpp', 'ui/application/main-menu.rml'):
        source = ROOT / relative
        if source.exists():
            characters.update(character for character in source.read_text(encoding='utf-8') if ord(character) >= 0x80)
    return ''.join(sorted(characters))


def main():
    result = inspect_font(Path(__file__).with_name('NotoSerifCJKsc-Regular.otf'), menu_characters())
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
