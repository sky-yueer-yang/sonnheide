#!/usr/bin/env python3
"""Finite ADR0018 arithmetic, event semantics and actual binary checkpoint model.

Python 3.9+ standard library. This is not the production simulation, GPU or a
complete GSHHG importer. run() is executed only by the root's batch audit.
"""
import hashlib
import itertools
import json
import math
import re
import struct
import unicodedata
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path
from typing import Dict, List, Tuple

HEIGHTS = {'deep_ocean': -20000, 'close_ocean': -8000,
           'shallow_water': -2000, 'sand': 1000, 'soil': 2000,
           'hill': 16000, 'mountain': 48000, 'high_peak': 96000}
TIERS = tuple(HEIGHTS)
CELL_MM = 2000
RAMP_Q = 125
CLOCK_DAY = 12000
CLOCK_HOUR = 500
HEADER = struct.Struct('<8sHHIIIQQ16s32s32s')
ENTRY = struct.Struct('<IIQQQ32s')
REQUIRED_SEGMENTS = tuple(range(1, 9))
OPTIONAL_PRESENTATION = 0x80000001
PHASES = ('FREEZE', 'INTEGRATE_OLD', 'MARK_DUE_DEATH',
          'APPLY_BOUNDARY_COMMANDS', 'DELIVER_AND_FREEZE_DUE',
          'PREPARE_PHYSICAL', 'RESOLVE_PHYSICAL', 'ACTUAL_INTERACTIONS',
          'DUE_EXECUTION', 'FINAL_LIFECYCLE', 'PUBLISH')
UINT64_RANGE = 1 << 64
RNG_MAGIC = b'SONNRNG1'
RUNTIME_CONTRACT = json.loads((Path(__file__).resolve().parents[3] /
                              'data/contracts/runtime_foundation_v1.json').read_text())
CANONICAL_KIND_REGISTRY = RUNTIME_CONTRACT['object_ref_wire']['canonical_kind_registry']
RNG_KINDS = {kind for kind, record in CANONICAL_KIND_REGISTRY.items()
             if record['scope'] not in RUNTIME_CONTRACT['counter_rng']['excluded_subject_scopes']}


def rng_key(seed, subject, domain, definition_hash, event_counter,
            draw_index, rejection_attempt=0):
    """Exact SONNRNG1 preimage; this pure function consumes no World state."""
    if type(seed) is not bytes or len(seed) != 32 or \
            type(definition_hash) is not bytes or len(definition_hash) != 32:
        raise ValueError('RNG_SEED_OR_DEFINITION_BYTES')
    if type(domain) is not str or re.fullmatch(r'[a-z][a-z0-9_.]{0,63}', domain) is None:
        raise ValueError('RNG_DOMAIN')
    if type(subject) is not dict or set(subject) != {'world_id', 'kind', 'stable_id', 'generation'}:
        raise ValueError('RNG_CANONICAL_REF')
    world, kind, identity, generation = (subject[k] for k in
                                         ('world_id', 'kind', 'stable_id', 'generation'))
    if type(world) is not str or re.fullmatch(r'[0-9a-f]{32}', world) is None or \
            type(identity) is not str or re.fullmatch(r'[0-9a-f]{16}', identity) is None or \
            type(kind) is not str or re.fullmatch(r'[a-z][a-z0-9_]{0,63}', kind) is None or \
            kind not in RNG_KINDS or \
            type(generation) is not int or not 1 <= generation <= 4294967295:
        raise ValueError('RNG_CANONICAL_REF')
    for value in (event_counter, draw_index, rejection_attempt):
        if type(value) is not int or not 0 <= value < UINT64_RANGE:
            raise ValueError('RNG_COUNTER_RANGE')
    domain_bytes, kind_bytes = domain.encode('ascii'), kind.encode('ascii')
    return (RNG_MAGIC + seed + struct.pack('<H', len(domain_bytes)) + domain_bytes +
            bytes.fromhex(world) + struct.pack('<H', len(kind_bytes)) + kind_bytes +
            struct.pack('<QI', int(identity, 16), generation) + definition_hash +
            struct.pack('<QQQ', event_counter, draw_index, rejection_attempt))


def uniform_from_raw(bound, raw_at_attempt, attempt_limit=4096):
    """Unbiased bounded integer and consumed-attempt count, no modulo bias."""
    if type(bound) is not int or not 1 <= bound < (1 << 63):
        raise ValueError('RNG_BOUND_RANGE')
    limit = (UINT64_RANGE // bound) * bound
    for attempt in range(attempt_limit):
        raw = raw_at_attempt(attempt)
        if type(raw) is not int or not 0 <= raw < UINT64_RANGE:
            raise ValueError('RNG_RAW_RANGE')
        if raw < limit:
            return raw % bound, attempt + 1
    raise ValueError('RNG_REJECTION_LIMIT')


def rng_uniform(seed, subject, domain, definition_hash, event_counter,
                draw_index, bound):
    if type(draw_index) is not int or not 0 <= draw_index < UINT64_RANGE - 1:
        raise ValueError('RNG_COUNTER_OVERFLOW')
    def raw(attempt):
        key = rng_key(seed, subject, domain, definition_hash, event_counter,
                      draw_index, attempt)
        return int.from_bytes(hashlib.sha256(key).digest()[:8], 'little')
    value, consumed_attempts = uniform_from_raw(bound, raw)
    return value, draw_index + 1, consumed_attempts


def round_even(value: Fraction) -> int:
    q, rem = divmod(value.numerator, value.denominator)
    twice = 2 * rem
    return q + (twice > value.denominator or
                (twice == value.denominator and q % 2 != 0))


def tier_change(kind: str, delta: int) -> str:
    if delta not in (-1, 1) or kind not in HEIGHTS:
        raise ValueError('INVALID_TIER_COMMAND')
    index = TIERS.index(kind) + delta
    if not 0 <= index < len(TIERS):
        raise ValueError('AT_TIER_LIMIT')
    return TIERS[index]


def circular_hit(cx: int, cy: int, radius: int,
                 cell_x: int, cell_y: int) -> bool:
    if radius <= 0:
        raise ValueError('INVALID_RADIUS')
    low_x, low_y = cell_x * CELL_MM, cell_y * CELL_MM
    dx = max(low_x - cx, 0, cx - (low_x + CELL_MM))
    dy = max(low_y - cy, 0, cy - (low_y + CELL_MM))
    return dx * dx + dy * dy < radius * radius


def on_segment(p, a, b) -> bool:
    cross = (p[0] - a[0]) * (b[1] - a[1]) - (p[1] - a[1]) * (b[0] - a[0])
    return cross == 0 and min(a[0], b[0]) <= p[0] <= max(a[0], b[0]) and \
        min(a[1], b[1]) <= p[1] <= max(a[1], b[1])


def inside_even_odd(p, ring) -> bool:
    inside = False
    for a, b in zip(ring, ring[1:] + ring[:1]):
        if on_segment(p, a, b):
            return True
        if (a[1] > p[1]) != (b[1] > p[1]):
            crossing_x = Fraction(a[0]) + Fraction(p[1] - a[1], b[1] - a[1]) * (b[0] - a[0])
            if crossing_x > p[0]:
                inside = not inside
    return inside


def source_land(p, rings) -> bool:
    hits = [(level, polygon_id) for level, polygon_id, ring in rings
            if inside_even_odd(p, ring)]
    if not hits:
        return False
    level, _ = min(hits, key=lambda pair: (-pair[0], pair[1]))
    return level % 2 == 1


def unwrap_longitude(value: int, west: int) -> int:
    return west + ((value - west) % 360000000)


def earth_centers(west, east_unwrapped, south, north, nx):
    width, height = east_unwrapped - west, north - south
    if not -80000000 <= south < north <= 80000000:
        raise ValueError('POLAR_OR_INVALID_LATITUDE')
    if not 0 < width <= 60000000 or not 0 < height <= 60000000:
        raise ValueError('INVALID_SOURCE_SPAN')
    if not 32 <= nx <= 1008:
        raise ValueError('INVALID_CORE_SIZE')
    ny = (nx * height + width - 1) // width
    if not 32 <= ny <= 1008:
        raise ValueError('INVALID_CORE_SIZE')
    return [[(Fraction(west) + Fraction((2 * i + 1) * width, 2 * nx),
              Fraction(south) + Fraction((2 * j + 1) * width, 2 * nx))
             for i in range(nx)] for j in range(ny)]


def clip_wet_triangle(triangle):
    """Return exact (x,y,h) vertices for strict negative-height wet region."""
    result = []
    for a, b in zip(triangle, triangle[1:] + triangle[:1]):
        a_wet, b_wet = a[2] < 0, b[2] < 0
        if a_wet:
            result.append(tuple(Fraction(v) for v in a))
        if a_wet != b_wet:
            t = Fraction(a[2], a[2] - b[2])
            result.append((a[0] + t * (b[0] - a[0]),
                           a[1] + t * (b[1] - a[1]), Fraction(0)))
    return result


def area2(poly):
    return abs(sum(a[0] * b[1] - a[1] * b[0]
                   for a, b in zip(poly, poly[1:] + poly[:1]))) if poly else Fraction(0)


def edge_wet_interval(height0, height1):
    a, b = Fraction(height0), Fraction(height1)
    if a >= 0 and b >= 0:
        return None
    if a < 0 and b < 0:
        return (Fraction(0), Fraction(1))
    crossing = -a / (b - a)
    return (Fraction(0), crossing) if a < 0 else (crossing, Fraction(1))


def shared_positive_interval(a, b):
    return a is not None and b is not None and max(a[0], b[0]) < min(a[1], b[1])


def temporary_components(nodes, adjacency, revision):
    """No persistent WaterBodyRef, ancestry, name or split/merge state."""
    unseen = set(nodes)
    result = {}
    while unseen:
        first = min(unseen)
        pending, connected = [first], set()
        while pending:
            node = pending.pop()
            if node in connected:
                continue
            connected.add(node)
            pending.extend(sorted((set(adjacency.get(node, ())) & unseen) - connected,
                                  reverse=True))
        unseen -= connected
        key = (revision, min(connected))
        for node in connected:
            result[node] = key
    return result


def straight_ramp(start_kind, end_kind, cells, grade):
    if start_kind not in HEIGHTS or end_kind not in HEIGHTS or cells < 1:
        raise ValueError('INVALID_RAMP_INPUT')
    start, end = HEIGHTS[start_kind], HEIGHTS[end_kind]
    if min(start, end) < 0 or max(start, end) >= 80000:
        raise ValueError('INVALID_RAMP_LANDING')
    start_r, rise_r = start // RAMP_Q, (end - start) // RAMP_Q
    corners = [start_r + round_even(Fraction(rise_r * i, cells))
               for i in range(cells + 1)]
    if any(Fraction(abs(b - a) * RAMP_Q, CELL_MM) > grade
           for a, b in zip(corners, corners[1:])):
        raise ValueError('RAMP_GRADE_AFTER_QUANTIZATION')
    return tuple(corners)


def reject_nonadjacent_overlap(rects):
    for i, a in enumerate(rects):
        for j, b in enumerate(rects):
            if j <= i + 1:
                continue
            if max(a[0], b[0]) < min(a[2], b[2]) and max(a[1], b[1]) < min(a[3], b[3]):
                raise ValueError('RAMP_SELF_OVERLAP')


@dataclass(frozen=True)
class BoundaryCommand:
    sequence: int
    command_id: str
    effect: str


def boundary_tick(commands, stock, carried, receipt_ids):
    """Finite God wipe/pickup race, distinct from same-instant hit aggregation."""
    current_stock, current_carried = stock, carried
    receipts = set(receipt_ids)
    log = []
    for command in sorted(commands, key=lambda c: (3 if c.effect == 'wipe' else 7, c.sequence, c.command_id)):
        if command.command_id in receipts:
            continue
        if command.effect == 'wipe':
            current_stock = 0  # God geography ground wipe; body-bound subtree survives.
            receipts.add(command.command_id)
            log.append('wipe')
        elif command.effect == 'pickup' and current_stock > 0:
            current_stock -= 1
            current_carried += 1
            receipts.add(command.command_id)
            log.append('pickup')
    return (current_stock, current_carried, tuple(sorted(receipts)), tuple(log))


def simultaneous_hits(health, legal_shots):
    """Already frozen same-instant shots survive the order of their equal-time hits."""
    damages = {actor: 0 for actor in health}
    fired = []
    receipts = set()
    for shot_id, shooter, target, severity in sorted(legal_shots):
        if shot_id in receipts or health.get(shooter, 0) <= 0:
            continue
        receipts.add(shot_id)
        fired.append(shot_id)
        damages[target] = damages.get(target, 0) + severity
    final = {actor: max(0, hp - damages.get(actor, 0)) for actor, hp in health.items()}
    dead = tuple(sorted(actor for actor, hp in final.items() if hp == 0 and health[actor] > 0))
    return final, tuple(fired), dead


def accepted_ballots(ballots, deadline, eligible):
    return tuple(sorted(actor for actor, received_tick in ballots
                        if received_tick <= deadline and actor in eligible))


def no_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('DUPLICATE_JSON_KEY')
        result[key] = value
    return result


def validate_value(value, depth=0):
    if depth > 64:
        raise ValueError('JSON_DEPTH')
    if type(value) is str:
        if unicodedata.normalize('NFC', value) != value or len(value.encode('utf-8')) > 65536:
            raise ValueError('INVALID_STRING')
    elif type(value) is int:
        if not -(1 << 63) <= value < (1 << 63):
            raise ValueError('INT64_RANGE')
    elif value is None or type(value) is bool:
        pass
    elif type(value) is list:
        for item in value:
            validate_value(item, depth + 1)
    elif type(value) is dict:
        for key, item in value.items():
            if type(key) is not str:
                raise ValueError('NONSTRING_KEY')
            validate_value(key, depth + 1)
            validate_value(item, depth + 1)
    else:
        raise ValueError('NONINTEGER_JSON_NUMBER_OR_TYPE')


def canonical(value):
    validate_value(value)
    return json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(',', ':'), allow_nan=False).encode('utf-8')


def reject_float(value):
    raise ValueError('FLOAT_NOT_ALLOWED')


def encode_checkpoint(segments, definition_hash, tick=0, revision=1):
    if len(segments) > 32:
        raise ValueError('TOO_MANY_SEGMENTS')
    chunks = []
    offset = HEADER.size + ENTRY.size * len(segments)
    table = bytearray()
    for kind, payload in sorted(segments.items()):
        data = canonical(payload)
        count = len(payload.get('records', []))
        table.extend(ENTRY.pack(kind, 1, offset, len(data), count,
                                hashlib.sha256(data).digest()))
        chunks.append(data)
        offset += len(data)
    header = HEADER.pack(b'SONNSAV1', 1, 0, 0, len(segments), 0,
                         tick, revision, bytes.fromhex('0123456789abcdef' * 2),
                         bytes.fromhex(definition_hash), hashlib.sha256(table).digest())
    return header + bytes(table) + b''.join(chunks)


def decode_checkpoint(blob, known_definition_hashes):
    if len(blob) < HEADER.size or len(blob) > 268435456:
        raise ValueError('FILE_LENGTH')
    magic, major, minor, flags, count, reserved, tick, revision, world, definition_hash, table_hash = HEADER.unpack_from(blob)
    if magic != b'SONNSAV1' or (major, minor, flags, reserved) != (1, 0, 0, 0):
        raise ValueError('UNSUPPORTED_HEADER')
    if tick >= (1 << 63) or revision >= (1 << 63):
        raise ValueError('HEADER_INT64_RANGE')
    if not 8 <= count <= 32 or definition_hash.hex() not in known_definition_hashes:
        raise ValueError('COUNT_OR_MISSING_DEFINITION')
    data_start = HEADER.size + count * ENTRY.size
    if data_start > len(blob):
        raise ValueError('TRUNCATED_TABLE')
    table = blob[HEADER.size:data_start]
    if hashlib.sha256(table).digest() != table_hash:
        raise ValueError('BAD_TABLE_HASH')
    result, expected_offset, last_kind = {}, data_start, -1
    for index in range(count):
        kind, schema, offset, length, records, digest = ENTRY.unpack_from(table, index * ENTRY.size)
        if kind <= last_kind or schema != 1 or offset != expected_offset:
            raise ValueError('UNSORTED_DUPLICATE_SCHEMA_OR_OFFSET')
        if length > 134217728 or records > 1000000 or offset + length > len(blob):
            raise ValueError('SEGMENT_BOUNDS')
        payload = blob[offset:offset + length]
        if hashlib.sha256(payload).digest() != digest:
            raise ValueError('BAD_PAYLOAD_HASH')
        if kind not in REQUIRED_SEGMENTS and kind < 0x80000000:
            raise ValueError('UNKNOWN_REQUIRED_SEGMENT')
        if kind in REQUIRED_SEGMENTS or kind == OPTIONAL_PRESENTATION:
            data = json.loads(payload.decode('utf-8'), object_pairs_hook=no_duplicates,
                              parse_float=reject_float, parse_constant=reject_float)
            if canonical(data) != payload or type(data) is not dict:
                raise ValueError('NONCANONICAL_PAYLOAD')
            if len(data.get('records', [])) != records:
                raise ValueError('RECORD_COUNT_MISMATCH')
            result[kind] = data
        expected_offset = offset + length
        last_kind = kind
    if expected_offset != len(blob) or not set(REQUIRED_SEGMENTS).issubset(result):
        raise ValueError('TAIL_OR_MISSING_REQUIRED_SEGMENT')
    for column in result[2].get('records', []):
        if column.get('shape') == 'flat':
            kind = column.get('kind')
            if kind not in HEIGHTS or ('height_mm' in column and column['height_mm'] != HEIGHTS[kind]):
                raise ValueError('ILLEGAL_FLAT_HEIGHT')
        elif column.get('shape') == 'ramp_patch':
            corners = column.get('cornerHeightR', [])
            if len(corners) != 4 or any(type(v) is not int or not -160 <= v <= 768 for v in corners):
                raise ValueError('ILLEGAL_RAMP_CORNERS')
        else:
            raise ValueError('UNKNOWN_SURFACE_SHAPE')
    deterministic = hashlib.sha256(b''.join(canonical(result[k]) for k in REQUIRED_SEGMENTS)).hexdigest()
    return {'tick': tick, 'revision': revision, 'world': world.hex(),
            'segments': result, 'deterministic_hash': deterministic}


def durable_publish(old_pointer, new_name, fail_stage=None):
    stages = ('write_temp', 'flush_file', 'readback', 'rename_checkpoint',
              'flush_checkpoint', 'write_pointer', 'flush_pointer', 'before_pointer_replace')
    seen = []
    pointer = old_pointer
    for stage in stages:
        if stage == fail_stage:
            return pointer, tuple(seen), False
        seen.append(stage)
        if stage == 'before_pointer_replace':
            pointer = new_name
    return pointer, tuple(seen), True


def uncertain_directory_flush(old_pointer, new_pointer):
    # After rename the OS may expose new bytes. This is not a rollback proof.
    return {'status': 'DURABILITY_UNKNOWN', 'in_memory_continue': old_pointer,
            'visible_disk_pointer': new_pointer, 'publish_world': False,
            'delete_previous_checkpoint': False, 'automatic_new_continue': False}


def run():
    checks = []

    def check(name, predicate):
        if not predicate:
            raise AssertionError(name)
        checks.append(name)

    def fails(name, operation):
        try:
            operation()
        except (ValueError, UnicodeDecodeError, struct.error, json.JSONDecodeError):
            checks.append(name)
        else:
            raise AssertionError(name)

    check('binary_header_actual_120_bytes', HEADER.size == 120)
    check('binary_entry_actual_64_bytes', ENTRY.size == 64)
    check('sha256_primitive_abc_known_vector', hashlib.sha256(b'abc').hexdigest() ==
          'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
    rng_subject = {'world_id': '11' * 16, 'kind': 'actor',
                   'stable_id': '0000000000000002', 'generation': 3}
    rng_seed, rng_definition = bytes(range(32)), bytes.fromhex('aa' * 32)
    rng_preimage = rng_key(rng_seed, rng_subject, 'birth', rng_definition, 4, 5)
    expected_preimage_hex = (
        '534f4e4e524e4731'
        '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f'
        '05006269727468' + '11' * 16 + '05006163746f72'
        '020000000000000003000000' + 'aa' * 32 +
        '040000000000000005000000000000000000000000000000')
    check('counter_rng_exact_length_prefixed_canonical_byte_vector',
          rng_preimage.hex() == expected_preimage_hex and len(rng_preimage) == 138)
    check('counter_rng_rejection_removes_uint64_modulo_bias',
          uniform_from_raw(10, lambda attempt: UINT64_RANGE - 1 if attempt == 0 else 17) == (7, 2))
    check('counter_rng_bound_one_is_zero', uniform_from_raw(1, lambda attempt: UINT64_RANGE - 1) == (0, 1))
    fails('counter_rng_rejection_limit_is_error_no_reseed',
          lambda: uniform_from_raw(10, lambda attempt: UINT64_RANGE - 1, 3))
    fails('counter_rng_boolean_bound_not_integer', lambda: uniform_from_raw(True, lambda attempt: 0))
    first_draw = rng_uniform(rng_seed, rng_subject, 'birth', rng_definition, 4, 5, 10000)
    replay_draw = rng_uniform(rng_seed, dict(rng_subject), 'birth', rng_definition, 4, 5, 10000)
    check('counter_rng_saved_event_counter_draw_index_replays_exactly', first_draw == replay_draw and first_draw[1] == 6)
    saved_draw_index = first_draw[1]
    check('counter_rng_resume_uses_saved_next_index',
          rng_uniform(rng_seed, rng_subject, 'birth', rng_definition, 4, saved_draw_index, 10000) ==
          rng_uniform(rng_seed, rng_subject, 'birth', rng_definition, 4, 6, 10000))
    check('counter_rng_domain_and_event_have_distinct_preimages',
          rng_key(rng_seed, rng_subject, 'culture.daily', rng_definition, 4, 5) != rng_preimage and
          rng_key(rng_seed, rng_subject, 'birth', rng_definition, 5, 5) != rng_preimage)
    aliased_subject = dict(rng_subject); aliased_subject['kind'] = 'person'
    fails('counter_rng_rejects_view_alias_to_prevent_second_stream',
          lambda: rng_key(rng_seed, aliased_subject, 'birth', rng_definition, 4, 5))
    bogus_subject = dict(rng_subject); bogus_subject['kind'] = 'bogus'
    fails('counter_rng_rejects_unregistered_regex_valid_kind',
          lambda: rng_key(rng_seed, bogus_subject, 'birth', rng_definition, 4, 5))
    local_subject = dict(rng_subject); local_subject['kind'] = 'save_slot'
    fails('counter_rng_local_profile_has_no_simulation_stream',
          lambda: rng_key(rng_seed, local_subject, 'birth', rng_definition, 4, 5))
    check('eight_flat_heights_are_distinct', len(set(HEIGHTS.values())) == 8)
    check('sand_soil_real_geography_height_difference', HEIGHTS['soil'] - HEIGHTS['sand'] == 1000)
    check('raise_exact_one_tier', tier_change('soil', 1) == 'hill')
    fails('highest_tier_does_not_create_free_height', lambda: tier_change('high_peak', 1))
    fails('lowest_tier_does_not_create_free_height', lambda: tier_change('deep_ocean', -1))
    check('ties_to_even_positive_negative', [round_even(Fraction(v, 2)) for v in [-5, -3, -1, 1, 3, 5]] == [-2, -2, 0, 0, 2, 2])
    check('zero_area_circular_edge_does_not_hit', not circular_hit(-2000, 1000, 2000, 0, 0))
    check('positive_area_circle_hits', circular_hit(-1999, 1000, 2000, 0, 0))
    rings = [(1, 1, [(0, 0), (10, 0), (10, 10), (0, 10)]),
             (2, 2, [(2, 2), (8, 2), (8, 8), (2, 8)]),
             (3, 3, [(3, 3), (7, 3), (7, 7), (3, 7)]),
             (4, 4, [(4, 4), (6, 4), (6, 6), (4, 6)])]
    check('deepest_source_layer_not_map_color', [source_land((x, x), rings) for x in [1, Fraction(5, 2), Fraction(7, 2), 5, 11]] == [True, False, True, False, False])
    check('ring_edge_exact_sample_is_defined', inside_even_odd((0, 5), rings[0][2]))
    centers = earth_centers(170000000, 190000000, -10000000, 10000000, 32)
    check('date_line_one_unwrapped_core', len(centers) == 32 and centers[0][-1][0] > 180000000)
    check('date_line_source_wrap', unwrap_longitude(-179000000, 170000000) == 181000000)
    check('highest_preview_grid_same_rational_centers', centers == earth_centers(170000000, 190000000, -10000000, 10000000, 32))
    fails('polar_selection_rejected', lambda: earth_centers(0, 20000000, 70000000, 90000000, 32))
    nw, ne, se, sw = (0, 0, 1000), (2, 0, -1000), (2, 2, 1000), (0, 2, -1000)
    check('two_triangle_wet_regions_have_actual_area', area2(clip_wet_triangle([nw, ne, se])) > 0 and area2(clip_wet_triangle([nw, se, sw])) > 0)
    check('dry_diagonal_does_not_join_wet_regions', edge_wet_interval(nw[2], se[2]) is None)
    check('point_only_wet_intersection_not_connected', not shared_positive_interval((Fraction(0), Fraction(1, 2)), (Fraction(1, 2), Fraction(1))))
    check('positive_length_wet_overlap_connected', shared_positive_interval((Fraction(0), Fraction(3, 4)), (Fraction(1, 2), Fraction(1))))
    separate = temporary_components([0, 1], {}, 1)
    merged = temporary_components([0, 1], {0: [1], 1: [0]}, 2)
    check('components_split_merge_have_no_persistent_identity', separate[0] != separate[1] and merged[0] == merged[1] and merged[0] != separate[0])
    walk = straight_ramp('soil', 'hill', 24, Fraction(1, 3))
    car = straight_ramp('soil', 'hill', 56, Fraction(1, 8))
    check('walk_real_quantized_ramp_endpoints', walk[0] * 125 == 2000 and walk[-1] * 125 == 16000)
    check('vehicle_actual_each_edge_one_eighth', all((b - a) * 125 == 250 for a, b in zip(car, car[1:])))
    fails('too_short_ramp_rejected_after_corner_quantization', lambda: straight_ramp('soil', 'hill', 55, Fraction(1, 8)))
    fails('peak_is_not_ramp_capability_exception', lambda: straight_ramp('soil', 'high_peak', 512, Fraction(1, 3)))
    fails('nonadjacent_route_overlap_rejected', lambda: reject_nonadjacent_overlap([(0, 0, 4, 10), (0, 10, 10, 14), (0, 0, 4, 10)]))
    orders = [BoundaryCommand(1, 'pickup', 'pickup'), BoundaryCommand(2, 'wipe', 'wipe')]
    expected = boundary_tick(orders, 1, 2, ())
    check('wipe_before_unperformed_pickup_preserves_carried', expected[:2] == (0, 2))
    permutations = 0
    for order in itertools.permutations(orders):
        check('boundary_command_permutation_%d' % permutations, boundary_tick(order, 1, 2, ()) == expected)
        permutations += 1
    got = boundary_tick([BoundaryCommand(1, 'pickup', 'pickup')], 1, 2, ())
    check('replayed_receipt_does_not_repeat_pickup', boundary_tick([BoundaryCommand(1, 'pickup', 'pickup')], got[0], got[1], got[2])[:2] == got[:2])
    shots = [('s2', 'b', 'a', 100), ('s1', 'a', 'b', 100)]
    health, fired, dead = simultaneous_hits({'a': 100, 'b': 100}, shots)
    check('same_instant_mutual_fire_both_real_deaths', health == {'a': 0, 'b': 0} and len(fired) == 2 and dead == ('a', 'b'))
    check('duplicate_shot_receipt_not_double_damage', simultaneous_hits({'a': 300, 'b': 100}, [shots[0], shots[0]])[0]['a'] == 200 and len(simultaneous_hits({'a': 300, 'b': 100}, [shots[0], shots[0]])[1]) == 1)
    check('exact_deadline_ballot_counted_without_late_votes', accepted_ballots([('a', 9), ('b', 10), ('c', 11)], 10, {'a', 'b', 'c'}) == ('a', 'b'))
    check('dead_representative_not_eligible', accepted_ballots([('a', 9), ('b', 10)], 10, {'a'}) == ('a',))
    check('calendar_and_motion_units_frozen', CLOCK_DAY == 24 * CLOCK_HOUR and 8 * CLOCK_HOUR == 4000)
    definition = hashlib.sha256(b'finite-definition-package').hexdigest()
    segments = {kind: {'records': []} for kind in REQUIRED_SEGMENTS}
    segments[2] = {'records': [{'shape': 'flat', 'kind': kind} for kind in TIERS]}
    binary = encode_checkpoint(segments, definition, 12000, 7)
    decoded = decode_checkpoint(binary, {definition})
    check('real_binary_checkpoint_roundtrip', decoded['tick'] == 12000 and decoded['revision'] == 7 and decoded['segments'][2] == segments[2])
    optional = dict(segments); optional[OPTIONAL_PRESENTATION] = {'records': [], 'camera_x': 123}
    check('presentation_segment_not_authoritative_world_hash', decode_checkpoint(encode_checkpoint(optional, definition, 12000, 7), {definition})['deterministic_hash'] == decoded['deterministic_hash'])
    fails('truncated_checkpoint_rejected', lambda: decode_checkpoint(binary[:-1], {definition}))
    fails('extra_trailing_bytes_rejected', lambda: decode_checkpoint(binary + b'X', {definition}))
    corrupt = bytearray(binary); corrupt[-1] ^= 1
    fails('bad_payload_hash_rejected', lambda: decode_checkpoint(bytes(corrupt), {definition}))
    fails('missing_definition_package_rejected', lambda: decode_checkpoint(binary, set()))
    fails('JSON_duplicate_keys_rejected', lambda: json.loads('{"x":1,"x":2}', object_pairs_hook=no_duplicates))
    fails('float_in_payload_rejected', lambda: canonical({'x': 0.5}))
    fails('non_NFC_string_rejected', lambda: canonical({'name': 'e\u0301'}))
    malformed = dict(segments); malformed[2] = {'records': [{'shape': 'flat', 'kind': 'soil', 'height_mm': 3500}]}
    fails('ninth_flat_height_rejected_after_binary_validation', lambda: decode_checkpoint(encode_checkpoint(malformed, definition), {definition}))
    malformed = dict(segments); malformed[2] = {'records': [{'shape': 'ramp_patch', 'cornerHeightR': [0, 0, 0]}]}
    fails('malformed_ramp_geometry_rejected', lambda: decode_checkpoint(encode_checkpoint(malformed, definition), {definition}))
    fault_stages = ('write_temp', 'flush_file', 'readback', 'rename_checkpoint', 'flush_checkpoint', 'write_pointer', 'flush_pointer', 'before_pointer_replace')
    for stage in fault_stages:
        check('checkpoint_failure_%s_preserves_continue' % stage, durable_publish('old', 'new', stage)[0] == 'old')
    check('checkpoint_success_advances_once_after_all_barriers', durable_publish('old', 'new')[0] == 'new')
    uncertain = uncertain_directory_flush('old', 'new')
    check('post_replace_flush_unknown_keeps_memory_and_old_checkpoint', uncertain['in_memory_continue'] == 'old' and uncertain['visible_disk_pointer'] == 'new' and not uncertain['publish_world'] and not uncertain['delete_previous_checkpoint'] and not uncertain['automatic_new_continue'])
    return {'passed': True, 'scope': 'finite_integer_rational_terrain_ramp_tick_counter_rng_and_binary_checkpoint_only',
            'named_check_count': len(checks), 'named_checks': checks,
            'boundary_command_permutation_cases': permutations,
            'checkpoint_fault_barriers': len(fault_stages),
            'counter_rng_vectors': [{'id': 'SONNRNG1_actor2_birth_counter4_draw5',
                                     'preimage_hex': rng_preimage.hex(),
                                     'sha256_hex': hashlib.sha256(rng_preimage).hexdigest(),
                                     'raw_u64_le_decimal': str(int.from_bytes(hashlib.sha256(rng_preimage).digest()[:8], 'little')),
                                     'uniform_10000': first_draw[0],
                                     'next_draw_index': first_draw[1],
                                     'consumed_attempts': first_draw[2]}],
            'canonical_registered_kind_count': len(CANONICAL_KIND_REGISTRY),
            'production_implemented': False,
            'not_proved': ['complete_GSHHG_import_and_general_ramp_solver', '3D_swept_collision',
                           'life_economy_and_warfare_runtime', 'actual_os_power_loss_durability',
                           'GPU_Renderer_Windows_or_Steam', 'measured_world_capacity']}


if __name__ == '__main__':
    print(json.dumps(run(), ensure_ascii=False, indent=2))
