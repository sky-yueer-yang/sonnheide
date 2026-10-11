"""Admission of original, material-based pixel UI artwork; Python 3.9+ stdlib."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import xml.etree.ElementTree as ET
import zlib

ROOT = Path(__file__).resolve().parents[1]

def check(value, message):
    if not value:
        raise ValueError(message)

def rgba_png(path):
    body = path.read_bytes()
    check(body[:8] == b'\x89PNG\r\n\x1a\n', 'Original PNG signature: ' + str(path))
    width, height, depth, colour, compression, filtering, interlace = struct.unpack('>IIBBBBB', body[16:29])
    check((depth, colour, compression, filtering, interlace) == (8, 6, 0, 0, 0), 'Exact RGBA8 PNG required')
    offset, compressed = 8, bytearray()
    while offset < len(body):
        length = struct.unpack_from('>I', body, offset)[0]
        kind, payload = body[offset + 4:offset + 8], body[offset + 8:offset + 8 + length]
        check(zlib.crc32(kind + payload) & 0xffffffff == struct.unpack_from('>I', body, offset + 8 + length)[0], 'Artwork PNG CRC')
        if kind == b'IDAT': compressed.extend(payload)
        offset += length + 12
    raw, stride = zlib.decompress(compressed), width * 4 + 1
    check(len(raw) == height * stride and all(raw[y * stride] == 0 for y in range(height)), 'Exact original PNG rows')
    return width, height, b''.join(raw[y * stride + 1:(y + 1) * stride] for y in range(height))

def admit(manifest_name):
    manifest = json.loads((ROOT / 'assets/manifests' / manifest_name).read_text(encoding='utf-8'))
    check(hashlib.sha256((ROOT / manifest['recipe']).read_bytes()).hexdigest() == manifest['recipe_sha256'], 'Artwork author recipe hash')
    for output in manifest['outputs']:
        path = ROOT / output['path']
        check(path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == output['sha256'], 'Artwork output changed: ' + str(path))
    return manifest

def validate():
    icons = admit('original_ui_icons.json')
    check(icons['format'] == 'SonnOriginalIcons9' and icons['logical_grid'] == 48 and icons['scaling'] == 'nearest_integer' and icons['alpha'] == 'binary', '48-cell original heavy pixel objects required')
    check(icons['edge_recipe']['outer_dark_cells'] == 2 and icons['edge_recipe']['white_contour_cells'] == 0, 'Heavy dark icon exterior required')
    check(icons['category_style'] == 'physical_pixel_object' and icons['yellow_base'] == '#ffaa00', 'Current material-based icon direction')
    bases = dict(icons['primary_colors'], **icons['material_base_colors'])
    for value, rule in icons['pigment_derivatives'].items():
        check(rule['base'] in bases and 0 <= rule['shade'] <= 1 and 0 <= rule['white_tint'] <= 1, 'Bounded material pigment recipe')
        base = tuple(int(bases[rule['base']][i:i + 2], 16) for i in (1, 3, 5))
        calculated = '#' + ''.join('%02x' % int(channel * rule['shade'] + (255 - channel * rule['shade']) * rule['white_tint'] + .5) for channel in base)
        check(calculated == value, 'Material pigment derivative is not registered truthfully')
    pngs, vectors = {}, {}
    for output in icons['outputs']:
        path = ROOT / output['path']
        name = path.stem if path.suffix == '.svg' else path.stem.rsplit('-', 1)[0]
        if path.suffix == '.svg': vectors[name] = path
        elif path.suffix == '.png': pngs.setdefault(name, {})[output['pixels']] = path
    check(len(pngs) == len(vectors) == 41 and pngs.keys() == vectors.keys(), 'Complete 41-icon vector and raster set')
    for name, versions in pngs.items():
        check(set(versions) == {96, 192}, 'Both integer-resolution icon variants: ' + name)
        logical = None
        for pixels, path in versions.items():
            width, height, raw = rgba_png(path)
            check(width == height == pixels, 'Icon dimensions')
            scale = pixels // 48
            current = b''.join(raw[(y * scale * width + x * scale) * 4:(y * scale * width + x * scale) * 4 + 4] for y in range(48) for x in range(48))
            check(all(raw[(y * width + x) * 4:(y * width + x) * 4 + 4] == current[((y // scale) * 48 + x // scale) * 4:((y // scale) * 48 + x // scale) * 4 + 4] for y in range(height) for x in range(width)), 'Noninteger icon pixels: ' + name)
            check(all(current[i] in (0, 255) for i in range(3, len(current), 4)), 'Nonbinary transparency')
            check(logical is None or current == logical, 'Icon variants disagree')
            logical = current
        painted = [(i % 48, i // 48) for i in range(48 * 48) if logical[i * 4 + 3]]
        colours = {'#' + logical[i * 4:i * 4 + 3].hex() for i in range(48 * 48) if logical[i * 4 + 3]}
        check(colours <= icons['pigment_derivatives'].keys() and len(colours) >= 7, 'Original material clusters must be registered: ' + name)
        exterior = icons['edge_recipe']['outer_color']
        for x, y in painted:
            pigment = '#' + logical[(y * 48 + x) * 4:(y * 48 + x) * 4 + 3].hex()
            channels = tuple(int(pigment[i:i + 2], 16) for i in (1, 3, 5))
            luminance = sum(a * b for a, b in zip(channels, (.2126, .7152, .0722)))
            check(luminance <= 220, 'Unbounded bright material highlight: ' + name)
            if icons['pigment_derivatives'][pigment]['base'] in ('blue', 'red', 'yellow', 'green', 'copper'):
                check(luminance <= 160.01, 'Vivid icon pigments must be deep rather than washed bright: ' + name)
            if pigment != exterior:
                for dy in range(-2, 3):
                    for dx in range(-2, 3):
                        if abs(dx) + abs(dy) <= 2:
                            xx, yy = x + dx, y + dy
                            check(0 <= xx < 48 and 0 <= yy < 48 and logical[(yy * 48 + xx) * 4 + 3] == 255, 'Actual two-cell icon exterior missing: ' + name)
        check(all(logical[(y * 48 + x) * 4 + 3] == 0 for y in range(48) for x in range(48) if x in (0, 47) or y in (0, 47)), 'Heavy icon edge clipped the transparent safety row: ' + name)
        check(painted and len(painted) >= 48 * 48 * .25 and len(painted) < 48 * 48, 'Object must be filled, with actual transparent background: ' + name)
        check(max(x for x, _ in painted) - min(x for x, _ in painted) + 1 >= 36 and max(y for _, y in painted) - min(y for _, y in painted) + 1 >= 36, 'Painted object is too small: ' + name)
        root = ET.fromstring(vectors[name].read_text(encoding='utf-8'))
        check(root.attrib.get('viewBox') == '0 0 48 48', 'Original vector pixel grid')
        reconstructed = bytearray(48 * 48 * 4)
        for cell in root:
            check(cell.tag.rsplit('}', 1)[-1] == 'rect' and cell.attrib.get('height') == '1', 'SVG must contain exact pixel rows')
            x, y, run = int(cell.attrib['x']), int(cell.attrib['y']), int(cell.attrib['width'])
            check(0 <= x < 48 and 0 <= y < 48 and run >= 1 and x + run <= 48 and re.fullmatch(r'#[0-9a-fA-F]{6}', cell.attrib['fill']), 'SVG pixel bounds/pigment')
            for column in range(x, x + run):
                offset = (y * 48 + column) * 4
                check(reconstructed[offset + 3] == 0, 'Overlapping SVG pixel')
                reconstructed[offset:offset + 4] = bytes.fromhex(cell.attrib['fill'][1:]) + b'\xff'
        check(bytes(reconstructed) == logical, 'SVG is not identical to the runtime icon: ' + name)
    materials = admit('original_ui_materials.json')
    expected = {'marble', 'brass', 'crimson', 'obsidian', 'wood', 'parchment', 'leather', 'iron', 'enamel'}
    check(materials['format'] == 'SonnOriginalUiMaterials1' and materials['sampling'] == 'nearest' and materials['logical_texel_dp'] == 1, 'Native point-sampled material direction')
    check(materials['default_dark_rim_dp'] == 3 and materials['default_dark_depth_dp'] == 3, 'Heavy physical material geometry')
    check({item['name'] for item in materials['outputs']} == expected and len(materials['outputs']) == 9, 'Complete original physical materials')
    for item in materials['outputs']:
        width, height, raw = rgba_png(ROOT / item['path'])
        check(width == height == item['width'] == item['height'] == 512, 'Fixed material texel dimensions')
        histogram = {}
        for i in range(0, len(raw), 4):
            check(raw[i + 3] == 255, 'Material tile opacity')
            colour = '#' + raw[i:i + 3].hex()
            check(colour in item['palette'], 'Unregistered material pigment')
            histogram[colour] = histogram.get(colour, 0) + 1
        check(histogram == item['histogram'] and len(histogram) >= 4, 'Real finite-palette material texture required')
        average = sum(sum(int(colour[i:i + 2], 16) for i in (1, 3, 5)) * n for colour, n in histogram.items()) / (width * height * 3)
        check(average < 110, 'UI material face must remain dark')
    # Exact non-mutating author recipe replay detects a self-consistently edited
    # manifest/PNG pair, not just an individual stale hash.
    spec = importlib.util.spec_from_file_location('sonn_original_materials', ROOT / materials['recipe'])
    recipe = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(recipe)
    for path, body in recipe.generated().items():
        check(path.read_bytes() == body, 'Material recipe replay differs: ' + str(path))
    spec = importlib.util.spec_from_file_location('sonn_original_icons', ROOT / icons['recipe'])
    recipe = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(recipe)
    recipe.generate(check=True)
    markup = (ROOT / 'assets/ui/application.rml').read_text(encoding='utf-8')
    for source in re.findall(r'src="\.\./generated/ui/icons/([^\"]+)"', markup):
        check((ROOT / 'assets/generated/ui/icons' / source).is_file(), 'Referenced production icon missing: ' + source)
    print('PASS 41 filled pixel objects: matching transparent SVG/96/192 PNG; nine dark physical textures and exact recipe replay')

if __name__ == '__main__': validate()
