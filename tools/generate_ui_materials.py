#!/usr/bin/env python3
"""Original pixel UI materials. Reproducible Python 3.9+ standard library.

The 512 x 512 texel tiles are artwork, not simulation state. Continuous noise
is evaluated only by this offline recipe, then reduced to a finite pigment
palette. Runtime rendering uses point sampling and cached tiled geometry.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/generated/ui/materials'
MANIFEST = ROOT / 'assets/manifests/original_ui_materials.json'
SIZE = 512
PALETTES = {
    'rock': ('#24272a', '#282b2e', '#2c2f32', '#303336', '#34373a', '#383b3e'),
    'brass': ('#1c1004', '#281a06', '#3b2407', '#503109', '#6a400b', '#824d0e', '#9a5c11', '#b07417', '#c68b25', '#d69b31', '#e4ab43'),
    'crimson': ('#220605', '#330c08', '#45120b', '#5b1a0f', '#712312', '#8b2b17', '#a4361c', '#ba4825', '#ce5a2e', '#dc723b'),
    'obsidian': ('#030507', '#050a0e', '#080f16', '#0b1620', '#0f1d29', '#152635', '#203243', '#304458'),
    'wood': ('#100905', '#1a1009', '#24180d', '#302010', '#3e2c15', '#4e391c', '#624722', '#795930', '#927143'),
    'parchment': ('#25190d', '#302211', '#3d2b14', '#4b361a', '#5a4120', '#6a4e28', '#7b5e32', '#8e703e', '#a5854c'),
    'leather': ('#11090b', '#1d0e10', '#2b1414', '#3b1c18', '#4e2720', '#66372a', '#814b35', '#a06746'),
    'iron': ('#090e12', '#111a21', '#1a2630', '#27333d', '#36434b', '#49565c', '#606d70', '#7a8584'),
    'enamel': ('#040b18', '#06172c', '#08223d', '#0b2f52', '#123f6a', '#195380', '#21659a', '#2c78ae', '#4090c2'),
}
# Explicit pins keep every existing material byte-identical when a family is
# replaced or reordered. Rock replaces only the original first family's seed.
SEEDS = {'rock': 1397706240, 'brass': 1397706377, 'crimson': 1397706514,
         'obsidian': 1397706651, 'wood': 1397706788, 'parchment': 1397706925,
         'leather': 1397707062, 'iron': 1397707199, 'enamel': 1397707336}


def rgb(value):
    return tuple(int(value[i:i + 2], 16) for i in (1, 3, 5))


def chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff)


def png(data, width=SIZE, height=SIZE):
    raw = b''.join(b'\0' + data[y * width * 4:(y + 1) * width * 4] for y in range(height))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')


def noise_grid(rng, cells):
    return [[rng.random() for _ in range(cells)] for _ in range(cells)]


def noise(grid, x, y):
    n = len(grid)
    u, v = x * n / SIZE, y * n / SIZE
    ix, iy = math.floor(u), math.floor(v)
    fx, fy = u - ix, v - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a = grid[iy % n][ix % n] * (1 - fx) + grid[iy % n][(ix + 1) % n] * fx
    b = grid[(iy + 1) % n][ix % n] * (1 - fx) + grid[(iy + 1) % n][(ix + 1) % n] * fx
    return a * (1 - fy) + b * fy


def draw_line(cells, points, pigment, thickness=1):
    # Integer line segments and toroidal wrapping create hard pixel cracks.
    # Adjacent segments join; no antialiasing or half-transparent edge texels.
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        steps = max(abs(x1 - x0), abs(y1 - y0), 1)
        for step in range(steps + 1):
            x = round(x0 + (x1 - x0) * step / steps)
            y = round(y0 + (y1 - y0) * step / steps)
            for oy in range(-(thickness // 2), thickness - thickness // 2):
                for ox in range(-(thickness // 2), thickness - thickness // 2):
                    cells[(y + oy) % SIZE][(x + ox) % SIZE] = pigment


def pigments(cells, palette):
    rgba = bytearray()
    histogram = {}
    for row in cells:
        for index in row:
            pigment = palette[index]
            histogram[pigment] = histogram.get(pigment, 0) + 1
            rgba.extend((*rgb(pigment), 255))
    return bytes(rgba), histogram


def dark_stone_metrics(cells, histogram):
    """Brightness and calmness measured from the actual finite-palette face."""
    palette = PALETTES['rock']
    colors = [rgb(pigment) for pigment in palette]
    luminance = [.2126 * red + .7152 * green + .0722 * blue for red, green, blue in colors]
    total = SIZE * SIZE
    weights = [histogram.get(pigment, 0) for pigment in palette]
    mean = sum(value * count for value, count in zip(luminance, weights)) / total
    deviation = math.sqrt(sum((value - mean) ** 2 * count for value, count in zip(luminance, weights)) / total)
    deltas, axis_means = [], []
    for vertical in (False, True):
        axis = []
        for y in range(SIZE):
            for x in range(SIZE):
                other = cells[(y + 1) % SIZE][x] if vertical else cells[y][(x + 1) % SIZE]
                axis.append(abs(luminance[cells[y][x]] - luminance[other]))
        axis_means.append(round(sum(axis) / len(axis), 6))
        deltas.extend(axis)
    deltas.sort()
    used = [luminance[i] for i, count in enumerate(weights) if count]
    return {'luminance_basis': 'encoded RGB weighted 0.2126R + 0.7152G + 0.0722B',
            'mean_rgb': [round(sum(color[channel] * count for color, count in zip(colors, weights)) / total, 6) for channel in range(3)],
            'luminance_mean': round(mean, 6), 'luminance_standard_deviation': round(deviation, 6),
            'luminance_minimum': round(min(used), 6), 'luminance_maximum': round(max(used), 6),
            'mean_saturation': round(sum((max(color) - min(color)) / max(color) * count for color, count in zip(colors, weights)) / total, 6),
            'horizontal_neighbor_delta_mean': axis_means[0], 'vertical_neighbor_delta_mean': axis_means[1],
            'neighbor_delta_p95': round(deltas[int((len(deltas) - 1) * .95)], 6),
            'neighbor_delta_over_12_fraction': round(sum(delta > 12 for delta in deltas) / len(deltas), 6),
            'texels_below_luminance_20': sum(count for value, count in zip(luminance, weights) if value < 20),
            'texels_above_luminance_80': sum(count for value, count in zip(luminance, weights) if value > 80)}


def rock_material():
    """One unbroken deep grey stone face with restrained mineral pixel grain.

    The entire face stays close to neutral, slightly cool #2c2f32 ground and
    #303336 mineral pigments. Periodic low-amplitude fields and sparse grain
    alter a few encoded RGB levels. There is no block partition, vein, stripe,
    or crack: boundary chips and inward cracks belong to actual UI geometry.
    """
    rng = random.Random(SEEDS['rock'])
    grids = [noise_grid(rng, n) for n in (4, 24, 128)]
    cells = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            broad = noise(grids[0], x, y)
            middle = noise(grids[1], x, y)
            fine = noise(grids[2], x, y)
            value = 2.7 + (broad - .5) * .6 + (middle - .5) * .42 + (fine - .5) * .36
            grain = rng.random()
            if grain < .035:
                value -= 1
            elif grain > .985:
                value += 1
            row.append(max(1, min(4, int(value))))
        cells.append(row)
    data, histogram = pigments(cells, PALETTES['rock'])
    return data, histogram, dark_stone_metrics(cells, histogram)


def material(name):
    if name == 'rock':
        data, histogram, _ = rock_material()
        return data, histogram
    rng = random.Random(SEEDS[name])
    grids = [noise_grid(rng, n) for n in (4, 12, 40)]
    palette = PALETTES[name]
    cells = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            broad = noise(grids[0], x, y)
            middle = noise(grids[1], x, y)
            fine = noise(grids[2], x, y)
            cloud = broad * .58 + middle * .3 + fine * .12
            if name == 'brass':
                # Hammered alloy, warm rather than shiny yellow plastic.
                value = 2.2 + cloud * 3.7 + (fine > .78) * .6
            elif name == 'crimson':
                value = 2.2 + cloud * 3.5
            elif name == 'obsidian':
                value = .4 + cloud * 3.2
            elif name == 'wood':
                grain = math.sin(y * math.tau * 14 / SIZE + broad * 9 + math.sin(x * math.tau / SIZE) * 3)
                value = 1.4 + cloud * 3.2 + grain * .6
            elif name == 'parchment':
                value = 2.2 + cloud * 3.2 + (fine > .7) * .3
            elif name == 'leather':
                grain = ((x * 13 + y * 17 + (x * y) % 19) % 23) / 23
                value = 1 + cloud * 3 + (grain > .83) * .6
            elif name == 'iron':
                value = 1.1 + cloud * 3.4 + ((y % 17 == 0) and fine > .65) * .6
            else:
                value = 1.8 + cloud * 3.8
            row.append(max(0, min(len(palette) - 1, int(value))))
        cells.append(row)
    if name in ('brass', 'iron', 'enamel', 'crimson', 'obsidian'):
        for scratch in range(24 if name in ('brass', 'iron') else 10):
            x, y = rng.randrange(SIZE), rng.randrange(SIZE)
            length = rng.randint(8, 36)
            p = 2 if name == 'obsidian' else 3 + scratch % 2
            draw_line(cells, [(x, y), (x + length, y - length // 5)], p)
            if name in ('brass', 'iron'):
                draw_line(cells, [(x, y + 1), (x + length, y + 1 - length // 5)], p + 1)
    elif name == 'wood':
        for _ in range(8):
            x0, y0 = rng.randrange(SIZE), rng.randrange(SIZE)
            for r in (5, 9, 14):
                points = [(x0 + round(math.cos(a * math.tau / 32) * r * 2.5), y0 + round(math.sin(a * math.tau / 32) * r)) for a in range(33)]
                draw_line(cells, points, 1 if r % 2 else 4)
    elif name == 'parchment':
        for _ in range(220):
            x, y = rng.randrange(SIZE), rng.randrange(SIZE)
            draw_line(cells, [(x, y), (x + rng.randint(1, 5), y + 1)], 3 if x % 2 else 5)
    elif name == 'leather':
        for _ in range(55):
            x, y = rng.randrange(SIZE), rng.randrange(SIZE)
            draw_line(cells, [(x, y), (x + 4, y + 1), (x + 7, y)], 2)
    return pigments(cells, palette)


def generated():
    outputs = {}
    records = []
    for name in PALETTES:
        if name == 'rock':
            data, histogram, metrics = rock_material()
        else:
            data, histogram = material(name)
        body = png(data)
        path = OUT / (name + '.png')
        outputs[path] = body
        record = {'name': name, 'path': path.relative_to(ROOT).as_posix(), 'sha256': hashlib.sha256(body).hexdigest(), 'width': SIZE, 'height': SIZE, 'seed': SEEDS[name], 'palette': list(PALETTES[name]), 'histogram': histogram}
        if name == 'rock':
            record['surface_recipe'] = {'algorithm': 'continuous deep grey stone slab face',
                                        'ground_pigments': ['#2c2f32', '#303336'],
                                        'tone': 'very dark neutral slightly cool grey',
                                        'detail': 'restrained fine mineral grain and very low-contrast tone variation',
                                        'periodicity': 'toroidal tone fields; pixel grain has no lines or seams',
                                        'antialiasing': False, 'interior_cracks': False,
                                        'block_partition': False, 'horizontal_stripes': False,
                                        'edge_damage_owner': 'actual control boundary geometry'}
            record['dark_stone_face'] = metrics
        records.append(record)
    manifest = {'format': 'SonnOriginalUiMaterials1', 'author': 'Sonnheide project', 'license': 'Original project artwork; no public license granted', 'recipe': 'tools/generate_ui_materials.py', 'recipe_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'logical_texel_dp': 1, 'sampling': 'nearest', 'alpha': 'opaque', 'default_dark_rim_dp': 3, 'default_dark_depth_dp': 3, 'color_direction': 'very dark neutral slightly cool grey stone; other eight dark saturated pigments unchanged', 'runtime': 'cached textured quads; fixed logical texel scale; no per-texel runtime geometry', 'outputs': records}
    outputs[MANIFEST] = (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
    return outputs


def contact_sheet(data):
    """Actual tile, 2x mineral-grain detail, and 2x2 repeat with point reduction."""
    width, height = SIZE * 3, SIZE
    result = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            if x < SIZE:
                sx, sy = x, y
            elif x < SIZE * 2:
                sx, sy = (x - SIZE) // 2 + 128, y // 2 + 128
            else:
                sx, sy = ((x - SIZE * 2) * 2) % SIZE, (y * 2) % SIZE
            source = (sy * SIZE + sx) * 4
            target = (y * width + x) * 4
            result[target:target + 4] = data[source:source + 4]
    return png(result, width, height)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Verify exact cached artwork and manifest without writing.')
    parser.add_argument('--preview', type=Path, help='Write an optional rock tile/detail/repeat QA contact sheet.')
    args = parser.parse_args()
    mismatches = []
    obsolete = OUT / 'marble.png'
    if obsolete.exists():
        if args.check:
            mismatches.append(obsolete.relative_to(ROOT).as_posix() + ' (obsolete)')
        else:
            obsolete.unlink()
    for path, body in generated().items():
        matches = path.is_file() and path.read_bytes() == body
        if not matches:
            if args.check:
                mismatches.append(path.relative_to(ROOT).as_posix())
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(body)
    if mismatches:
        raise SystemExit('Material cache differs from original recipe: ' + ', '.join(mismatches))
    if args.preview:
        data, _, _ = rock_material()
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        args.preview.write_bytes(contact_sheet(data))
    print('Original pixel materials: 9 finite-palette 512 x 512 tiles' + (' verified' if args.check else ' ready'))


if __name__ == '__main__':
    main()
