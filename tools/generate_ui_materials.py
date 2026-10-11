#!/usr/bin/env python3
"""Original dark pixel UI materials. Reproducible Python 3.9+ standard library.

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
    'marble': ('#0a1013', '#10181b', '#151e20', '#1b2527', '#222c2d', '#293334', '#34403e', '#424b44', '#555747', '#686447', '#7b7252'),
    'brass': ('#1c1004', '#281a06', '#3b2407', '#503109', '#6a400b', '#824d0e', '#9a5c11', '#b07417', '#c68b25', '#d69b31', '#e4ab43'),
    'crimson': ('#220605', '#330c08', '#45120b', '#5b1a0f', '#712312', '#8b2b17', '#a4361c', '#ba4825', '#ce5a2e', '#dc723b'),
    'obsidian': ('#030507', '#050a0e', '#080f16', '#0b1620', '#0f1d29', '#152635', '#203243', '#304458'),
    'wood': ('#100905', '#1a1009', '#24180d', '#302010', '#3e2c15', '#4e391c', '#624722', '#795930', '#927143'),
    'parchment': ('#25190d', '#302211', '#3d2b14', '#4b361a', '#5a4120', '#6a4e28', '#7b5e32', '#8e703e', '#a5854c'),
    'leather': ('#11090b', '#1d0e10', '#2b1414', '#3b1c18', '#4e2720', '#66372a', '#814b35', '#a06746'),
    'iron': ('#090e12', '#111a21', '#1a2630', '#27333d', '#36434b', '#49565c', '#606d70', '#7a8584'),
    'enamel': ('#040b18', '#06172c', '#08223d', '#0b2f52', '#123f6a', '#195380', '#21659a', '#2c78ae', '#4090c2'),
}
SEEDS = {name: 0x534f4e00 + i * 137 for i, name in enumerate(PALETTES)}


def rgb(value):
    return tuple(int(value[i:i + 2], 16) for i in (1, 3, 5))


def chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff)


def png(data):
    raw = b''.join(b'\0' + data[y * SIZE * 4:(y + 1) * SIZE * 4] for y in range(SIZE))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', SIZE, SIZE, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')


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


def material(name):
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
            if name == 'marble':
                value = 1 + cloud * 4.3
            elif name == 'brass':
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
    if name == 'marble':
        for vein in range(7):
            origin_x = rng.randrange(SIZE)
            points = []
            phase = rng.random() * math.tau
            second_phase = rng.random() * math.tau
            winding = vein % 2
            jitter = [rng.randint(-7, 7) for _ in range(SIZE // 8)]
            for y in range(-32, SIZE + 33, 8):
                # Integer torus winding plus periodic harmonics joins the
                # tile's opposite boundaries. Fractures never visibly stop
                # at an arbitrary texture edge on a tall material panel.
                x = origin_x + winding * y + round(math.sin(y * math.tau / SIZE + phase) * 43 + math.sin(y * math.tau * 3 / SIZE + second_phase) * 17) + jitter[(y // 8) % len(jitter)]
                points.append((x, y))
            draw_line(cells, points, 5 + vein % 3, 3)
            draw_line(cells, [(x - 1, y) for x, y in points], 8 + vein % 2)
            # Secondary fractures branch organically from the large vein.
            for branch in range(3):
                bx, by = points[rng.randrange(8, len(points) - 8)]
                direction = -1 if branch % 2 else 1
                branch_points = [(bx, by)]
                for _ in range(rng.randint(7, 16)):
                    bx += direction * rng.randint(3, 9)
                    by += rng.randint(1, 7)
                    branch_points.append((bx, by))
                draw_line(cells, branch_points, 6 + branch % 2)
        # Sparse calcite facets are 1–3 texel clusters, not white noise.
        for _ in range(90):
            x, y = rng.randrange(SIZE), rng.randrange(SIZE)
            for i in range(rng.randint(1, 3)):
                cells[y][(x + i) % SIZE] = 6
    elif name in ('brass', 'iron', 'enamel', 'crimson', 'obsidian'):
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
    rgba = bytearray()
    histogram = {}
    for row in cells:
        for index in row:
            pigment = palette[index]
            histogram[pigment] = histogram.get(pigment, 0) + 1
            rgba.extend((*rgb(pigment), 255))
    return bytes(rgba), histogram


def generated():
    outputs = {}
    records = []
    for name in PALETTES:
        data, histogram = material(name)
        body = png(data)
        path = OUT / (name + '.png')
        outputs[path] = body
        records.append({'name': name, 'path': path.relative_to(ROOT).as_posix(), 'sha256': hashlib.sha256(body).hexdigest(), 'width': SIZE, 'height': SIZE, 'seed': SEEDS[name], 'palette': list(PALETTES[name]), 'histogram': histogram})
    manifest = {'format': 'SonnOriginalUiMaterials1', 'author': 'Sonnheide project', 'license': 'Original project artwork; no public license granted', 'recipe': 'tools/generate_ui_materials.py', 'recipe_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'logical_texel_dp': 1, 'sampling': 'nearest', 'alpha': 'opaque', 'default_dark_rim_dp': 3, 'default_dark_depth_dp': 3, 'color_direction': 'dark saturated pigments without pale white-tinted enamel, crimson or brass', 'runtime': 'cached textured quads; fixed logical texel scale; no per-texel runtime geometry', 'outputs': records}
    outputs[MANIFEST] = (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
    return outputs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Verify exact cached artwork and manifest without writing.')
    args = parser.parse_args()
    mismatches = []
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
    print('Original pixel materials: 9 dark finite-palette 512 x 512 tiles' + (' verified' if args.check else ' ready'))


if __name__ == '__main__':
    main()
