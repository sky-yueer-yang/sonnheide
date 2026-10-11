"""Admission of original, material-based pixel UI artwork; Python 3.9+ stdlib."""
import hashlib
import importlib.util
import json
import math
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

def dark_stone_face_metrics(raw, width, height):
    # Derive admission measurements from decoded production pixels rather
    # than trusting the author's recipe parameters or manifest statistics.
    pixels = [tuple(raw[i:i + 3]) for i in range(0, len(raw), 4)]
    luminance = [.2126 * red + .7152 * green + .0722 * blue for red, green, blue in pixels]
    total = width * height
    mean = sum(luminance) / total
    deltas, axis_means = [], []
    for vertical in (False, True):
        axis = []
        for y in range(height):
            for x in range(width):
                other = ((y + 1) % height) * width + x if vertical else y * width + (x + 1) % width
                axis.append(abs(luminance[y * width + x] - luminance[other]))
        axis_means.append(round(sum(axis) / len(axis), 6))
        deltas.extend(axis)
    deltas.sort()
    return {'luminance_basis': 'encoded RGB weighted 0.2126R + 0.7152G + 0.0722B',
            'mean_rgb': [round(sum(pixel[channel] for pixel in pixels) / total, 6) for channel in range(3)],
            'luminance_mean': round(mean, 6),
            'luminance_standard_deviation': round(math.sqrt(sum((value - mean) ** 2 for value in luminance) / total), 6),
            'luminance_minimum': round(min(luminance), 6), 'luminance_maximum': round(max(luminance), 6),
            'mean_saturation': round(sum((max(pixel) - min(pixel)) / max(1, max(pixel)) for pixel in pixels) / total, 6),
            'horizontal_neighbor_delta_mean': axis_means[0], 'vertical_neighbor_delta_mean': axis_means[1],
            'neighbor_delta_p95': round(deltas[int((len(deltas) - 1) * .95)], 6),
            'neighbor_delta_over_12_fraction': round(sum(delta > 12 for delta in deltas) / len(deltas), 6),
            'texels_below_luminance_20': sum(value < 20 for value in luminance),
            'texels_above_luminance_80': sum(value > 80 for value in luminance)}

def validate():
    icons = admit('original_ui_icons.json')
    check(icons['format'] == 'SonnOriginalIcons10' and icons['logical_grid'] == 48 and icons['scaling'] == 'nearest_integer' and icons['alpha'] == 'binary', '48-cell original objects and exact rectangular toolbar glyphs required')
    check(icons['edge_recipe']['outer_dark_cells'] == 2 and icons['edge_recipe']['white_contour_cells'] == 0, 'Heavy dark icon exterior required')
    categories={'tab-'+name for name in ('camera','terrain','people','laws','buildings','economy','world','settings')}
    flat=categories | {'play','pause','toolbar-speed','toolbar-age'}
    check(icons['category_style'] == 'wide_flat_warm_white' and icons['yellow_base'] == '#ffaa00', 'Current flat toolbar and retained material-object direction')
    check(set(icons['category_icons']) == categories and set(icons['flat_glyphs']) == flat and icons['flat_glyph_colour'] == '#fff6df' and icons['category_logical_size'] == [48,28], 'Explicit independent warm-white toolbar glyph family')
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
    check(len(pngs) == len(vectors) == 43 and pngs.keys() == vectors.keys() and flat <= pngs.keys(), 'Complete43-icon vector and raster set')
    for name, versions in pngs.items():
        logical_height=28 if name in categories else 48
        check(set(versions) == {96, 192}, 'Both integer-resolution icon variants: ' + name)
        logical = None
        for pixels, path in versions.items():
            width, height, raw = rgba_png(path)
            check(width == pixels and height == pixels*logical_height//48, 'Icon dimensions and rectangular category aspect')
            scale = pixels // 48
            current = b''.join(raw[(y * scale * width + x * scale) * 4:(y * scale * width + x * scale) * 4 + 4] for y in range(logical_height) for x in range(48))
            check(all(raw[(y * width + x) * 4:(y * width + x) * 4 + 4] == current[((y // scale) * 48 + x // scale) * 4:((y // scale) * 48 + x // scale) * 4 + 4] for y in range(height) for x in range(width)), 'Noninteger icon pixels: ' + name)
            check(all(current[i] in (0, 255) for i in range(3, len(current), 4)), 'Nonbinary transparency')
            check(logical is None or current == logical, 'Icon variants disagree')
            logical = current
        painted = [(i % 48, i // 48) for i in range(48 * logical_height) if logical[i * 4 + 3]]
        colours = {'#' + logical[i * 4:i * 4 + 3].hex() for i in range(48 * logical_height) if logical[i * 4 + 3]}
        check(all(logical[(y * 48 + x) * 4 + 3] == 0 for y in range(logical_height) for x in range(48) if x in (0,47) or y in (0,logical_height-1)), 'Icon edge clipped the transparent safety row: ' + name)
        check(painted and len(painted) < 48*logical_height, 'Icon must have a real transparent silhouette: '+name)
        if name in flat:
            check(colours == {'#fff6df'}, 'Toolbar marks must use one warm-white pigment with no coloured or dark contour: '+name)
            if name in categories:
                width=max(x for x,y in painted)-min(x for x,y in painted)+1
                height=max(y for x,y in painted)-min(y for x,y in painted)+1
                check(width>=36 and 18<=height<=25 and width/height>=1.45, 'Category marks must be broad, flat and legible at their actual48x28 size: '+name)
            if name=='play':
                rows=[[x for x,y in painted if y==row] for row in sorted({y for x,y in painted})]
                check(len(rows)>=30 and len({min(row) for row in rows})==1 and all(row==list(range(min(row),max(row)+1)) for row in rows), 'Play must be a single solid triangle without an enclosing ring')
                widths=[len(row) for row in rows];peak=widths.index(max(widths))
                check(peak>8 and peak<len(widths)-8 and widths[:peak+1]==sorted(widths[:peak+1]) and widths[peak:]==sorted(widths[peak:],reverse=True), 'Play triangle must widen to one middle tip and taper back')
            if name=='pause':
                xs=sorted({x for x,y in painted});ys=sorted({y for x,y in painted})
                gaps=[index for index in range(1,len(xs)) if xs[index]>xs[index-1]+1]
                check(len(gaps)==1 and gaps[0]>=6 and len(xs)-gaps[0]==gaps[0] and len(ys)>=28 and len(painted)==len(xs)*len(ys), 'Pause must be exactly two equally thick filled bars with an open gap and no ring')
        else:
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
            check(len(painted)>=48*48*.25, 'Material object must remain filled: '+name)
            check(max(x for x, _ in painted) - min(x for x, _ in painted) + 1 >= 36 and max(y for _, y in painted) - min(y for _, y in painted) + 1 >= 36, 'Painted object is too small: ' + name)
        root = ET.fromstring(vectors[name].read_text(encoding='utf-8'))
        check(root.attrib.get('viewBox') == '0 0 48 '+str(logical_height), 'Original vector pixel grid')
        reconstructed = bytearray(48 * logical_height * 4)
        for cell in root:
            check(cell.tag.rsplit('}', 1)[-1] == 'rect' and cell.attrib.get('height') == '1', 'SVG must contain exact pixel rows')
            x, y, run = int(cell.attrib['x']), int(cell.attrib['y']), int(cell.attrib['width'])
            check(0 <= x < 48 and 0 <= y < logical_height and run >= 1 and x + run <= 48 and re.fullmatch(r'#[0-9a-fA-F]{6}', cell.attrib['fill']), 'SVG pixel bounds/pigment')
            for column in range(x, x + run):
                offset = (y * 48 + column) * 4
                check(reconstructed[offset + 3] == 0, 'Overlapping SVG pixel')
                reconstructed[offset:offset + 4] = bytes.fromhex(cell.attrib['fill'][1:]) + b'\xff'
        check(bytes(reconstructed) == logical, 'SVG is not identical to the runtime icon: ' + name)
    materials = admit('original_ui_materials.json')
    expected = {'rock', 'brass', 'crimson', 'obsidian', 'wood', 'parchment', 'leather', 'iron', 'enamel'}
    check(materials['format'] == 'SonnOriginalUiMaterials1' and materials['sampling'] == 'nearest' and materials['logical_texel_dp'] == 1, 'Native point-sampled material direction')
    check(materials['default_dark_rim_dp'] == 3 and materials['default_dark_depth_dp'] == 3, 'Heavy physical material geometry')
    check({item['name'] for item in materials['outputs']} == expected and len(materials['outputs']) == 9, 'Complete original physical materials')
    check(not (ROOT / 'assets/generated/ui/materials/marble.png').exists(), 'Historical marble must not remain in production artwork')
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
        if item['name'] == 'rock':
            channels = [sum(int(colour[i:i + 2], 16) * n for colour, n in histogram.items()) / (width * height) for i in (1, 3, 5)]
            check(all(40 <= channel <= 55 for channel in channels), 'Stone slab must have a very dark grey40–55RGB face')
            check(0 <= channels[1] - channels[0] <= 5 and 0 <= channels[2] - channels[1] <= 5 and channels[2] - channels[0] <= 8, 'Stone must be neutral slightly cool grey, without brown/yellow or vivid blue tint')
            for colour in item['palette']:
                pigment = [int(colour[i:i + 2], 16) for i in (1, 3, 5)]
                check(all(30 <= channel <= 70 for channel in pigment) and max(pigment) - min(pigment) <= 8, 'Stone palette must contain only dark low-saturation mineral greys')
            check('dark_stone_face' in item and 'pale_stone_face' not in item and 'fracture_metrics' not in item, 'Dark stone face needs current actual-pixel metrics, with obsolete style metrics removed')
            actual = dark_stone_face_metrics(raw, width, height)
            check(set(actual) == set(item['dark_stone_face']), 'Complete actual dark stone metric schema')
            for key, value in actual.items():
                claimed = item['dark_stone_face'][key]
                if isinstance(value, list):
                    check(len(claimed) == len(value) and all(abs(a - b) < .000002 for a, b in zip(value, claimed)), 'Stone actual-pixel metric differs: ' + key)
                elif isinstance(value, (int, float)):
                    check(abs(value - claimed) < .000002, 'Stone actual-pixel metric differs: ' + key)
                else:
                    check(value == claimed, 'Stone actual-pixel metric differs: ' + key)
            check(actual['luminance_standard_deviation'] <= 5 and actual['luminance_maximum'] - actual['luminance_minimum'] <= 25, 'Stone face must retain fine quiet grain rather than broad tiled cracks or contrasting blocks')
            check(actual['neighbor_delta_p95'] <= 8 and max(actual['horizontal_neighbor_delta_mean'], actual['vertical_neighbor_delta_mean']) <= 2, 'Stone mineral grain must stay restrained at the actual1dp texel scale')
            check(actual['texels_below_luminance_20'] == actual['texels_above_luminance_80'] == 0, 'Texture must not add black crack networks or bright mineral stripes; edge damage belongs to control geometry')
        else:
            check(average < 110, 'The other eight material faces retain their dark pigments')
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
    print('PASS31 retained coloured objects and12 flat warm-white glyphs: exact SVG/PNG pairs, broad categories and plainplay/pause;9 original textures and recipe replay')

if __name__ == '__main__': validate()
