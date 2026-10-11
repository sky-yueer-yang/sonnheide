#!/usr/bin/env python3
"""Finite design model, not game/runtime/GPU acceptance. Python 3.9+ stdlib.

Checks canonical editor dose under every packet partition, cross-bucket spacing,
all orders of finite planting/removal operations, mixed-profile normalized caps,
and duplicate receipts. Checks refusal/new-input increments with a rational sample alpha.
Does not model full canopy intersections, habitats,
access certificates, exponential lookup, real stocks, navigation or rendering.
"""
from fractions import Fraction
from itertools import combinations, permutations
import json


def canonical_dose(slots):
    result = Fraction(0)
    for duration_ms, normalized_distance in slots:
        if 0 <= normalized_distance < 1:
            result += Fraction(duration_ms, 1000) * (1-normalized_distance**2)**2
    return result


def all_partitions(seq):
    for mask in range(1 << max(0, len(seq)-1)):
        chunks, current = [], []
        for i, item in enumerate(seq):
            current.append(item)
            if i == len(seq)-1 or mask & (1 << i):
                chunks.append(tuple(current))
                current = []
        yield tuple(chunks)


# mm coordinates straddle x=8000 bucket edge. Radius is MATURE structure,
# deliberately distinct from tiny juvenile visual model radius.
CANDIDATES = {
    'a': (7900, 4000, 1500, 'pine'),
    'b': (8100, 4000, 1500, 'fruit'),
    'c': (12000, 4000, 1200, 'fruit'),
    'd': (4000, 4000, 1500, 'pine'),
    'e': (4000, 9000, 1800, 'pine'),
    'f': (11000, 9000, 1200, 'fruit'),
}
CAPS = {'pine': 3, 'fruit': 2}
MARGIN = 400


def compatible(first, second):
    x1, y1, r1, _ = CANDIDATES[first]
    x2, y2, r2, _ = CANDIDATES[second]
    return (x1-x2)**2+(y1-y2)**2 >= (r1+r2+MARGIN)**2


def quota(selected):
    return sum((Fraction(1, CAPS[CANDIDATES[k][3]]) for k in selected), Fraction(0))


def oracle_valid(selected):
    # Independent all-pair and normalized-rational oracle; no bucket pruning.
    return quota(selected) <= 1 and all(compatible(a,b) for a,b in combinations(selected,2))


class Model:
    def __init__(self):
        self.accepted = set()
        self.receipts = {}
        self.created = 0

    def plant(self, candidate, command_id):
        if command_id in self.receipts:
            return self.receipts[command_id]
        proposed = self.accepted | {candidate}
        # Global neighbor set; production may prune only with a proven full halo.
        ok = candidate not in self.accepted and quota(proposed) <= 1
        if ok:
            ok = all(compatible(candidate, other) for other in self.accepted)
        if ok:
            self.accepted.add(candidate)
            self.created += 1
        self.receipts[command_id] = ok
        return ok

    def remove(self, candidate):
        self.accepted.discard(candidate)


class IncrementalDensity:
    """Abstract dose response; alpha is an already-calibrated frozen lookup output.

    Exercises no historical deficit catch-up. It does NOT verify exp lookup.
    """
    def __init__(self, cap=32):
        self.cap, self.count, self.credit = cap, 0, Fraction(0)

    def apply_new_input(self, alpha, feasible=True):
        assert 0 <= alpha <= 1
        if not feasible or self.count == self.cap:
            self.credit = Fraction(0)
            return 0
        remaining = self.cap-self.count-self.credit
        proposed_credit = self.credit+remaining*alpha
        count = min(int(proposed_credit), self.cap-self.count)
        self.count += count
        self.credit = proposed_credit-count if self.count < self.cap else Fraction(0)
        assert 0 <= self.credit < 1
        return count

    def clear(self):
        self.count, self.credit = 0, Fraction(0)


def run():
    checks = 0
    slots = tuple((25, Fraction(i % 5, 5)) for i in range(12))
    exact = canonical_dose(slots)
    for packets in all_partitions(slots):
        # These are the SAME canonical input with varied render/network batching;
        # different sampled pointer histories are not asserted equal.
        assert sum((canonical_dose(p) for p in packets), Fraction(0)) == exact
        checks += 1
    assert canonical_dose(((1000, Fraction(0)),)) == 1
    assert canonical_dose(((1000, Fraction(1)),)) == 0
    checks += 2

    # All finite orderings, then replays with receipts, then all removals.
    for ordering in permutations(CANDIDATES):
        m = Model()
        for i, candidate in enumerate(ordering):
            command = ('stroke', i)
            m.plant(candidate, command)
            assert oracle_valid(m.accepted)
            snapshot = (m.accepted.copy(), m.created)
            m.plant(candidate, command)
            assert (m.accepted, m.created) == snapshot
            checks += 2
        for candidate in reversed(ordering):
            m.remove(candidate)
            assert oracle_valid(m.accepted)
            checks += 1
        assert not m.accepted
        checks += 1

    # Concrete counterexamples to superficially attractive approaches.
    assert CANDIDATES['a'][0]//8000 != CANDIDATES['b'][0]//8000
    assert not compatible('a', 'b')  # isolated bucket acceptance would collide
    checks += 2
    fake_per_species_full = {'a', 'd', 'e', 'c', 'f'}
    assert quota(fake_per_species_full) == 2  # separately full profiles double quota
    assert not oracle_valid(fake_per_species_full)
    checks += 2
    juvenile_r1 = juvenile_r2 = 50
    distance = abs(CANDIDATES['a'][0]-CANDIDATES['b'][0])
    assert distance >= juvenile_r1+juvenile_r2
    assert not compatible('a','b')  # young-only spacing fails mature fit
    checks += 2
    full_frame_dose = Fraction(25*144,1000)
    assert full_frame_dose != Fraction(25*30,1000)  # per-frame dt is wrong
    checks += 1
    # Long blocked hold cannot accumulate a historical target deficit.
    blocked = IncrementalDensity()
    fresh = IncrementalDensity()
    for _ in range(180):
        assert blocked.apply_new_input(Fraction(1,4), feasible=False) == 0
        assert blocked.count == 0 and blocked.credit == 0
        checks += 2
    assert blocked.apply_new_input(Fraction(0), feasible=True) == 0
    assert blocked.apply_new_input(Fraction(1,4)) == fresh.apply_new_input(Fraction(1,4))
    assert (blocked.count,blocked.credit) == (fresh.count,fresh.credit)
    checks += 3
    for _ in range(80):
        blocked.apply_new_input(Fraction(1,100))
        assert 0 <= blocked.count <= blocked.cap and 0 <= blocked.credit < 1
        checks += 1
    blocked.clear()
    assert blocked.count == 0 and blocked.credit == 0
    assert blocked.apply_new_input(Fraction(0)) == 0
    checks += 2
    assert Fraction(141422,100000)**2 > 2  # conservative diagonal octagon bound
    checks += 1
    return {'passed': True, 'checks': checks, 'canonical_partitions': 2048,
            'planting_orders': 720, 'scope': 'finite_dose_spacing_caps_receipts_and_rejection_new_input_policy',
            'production_gameplay_or_GPU_acceptance': False}


if __name__ == '__main__':
    print(json.dumps(run(), ensure_ascii=False, indent=2))
