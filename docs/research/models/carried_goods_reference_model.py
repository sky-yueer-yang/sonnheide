# -*- coding: utf-8 -*-
"""ADR0017 finite portable/ancestor wipe model; Python 3.9+ stdlib.

Not production code, spatial physics, animation or an inventory implementation.
Only explicit valid attachment/containment edges and frozen abstract hits.
"""
from dataclasses import dataclass, field
from copy import deepcopy
from itertools import product, permutations
import json

PORTABLE_EDGES = {'WORN', 'HELD', 'CARRIED'}
CONTENTS_EDGES = {'CONTAINED', 'ATTACHED_COMPONENT'}

@dataclass
class Entity:
    ref: int
    kind: str
    parent: object = None
    edge: str = 'NONE'
    alive: bool = False
    portable: bool = False
    quantity: int = 0
    owner: int = 900
    custody: int = 900
    durability: int = 73
    wetness: int = 17
    expiry: int = 550
    pose: int = 42
    chamber: int = 1
    reload_work: int = 7
    random_counter: int = 19

@dataclass
class Scene:
    objects: dict
    revision: int = 1
    receipts: set = field(default_factory=set)
    losses: dict = field(default_factory=dict)
    hazards: set = field(default_factory=set)

    def validate(self):
        for ref, obj in self.objects.items():
            if ref != obj.ref or obj.quantity < 0:
                raise ValueError('invalid identity or quantity')
            seen = {ref}
            node = obj
            while node.parent is not None:
                if node.parent not in self.objects or node.parent in seen:
                    raise ValueError('dangling or cyclic physical parent')
                seen.add(node.parent)
                node = self.objects[node.parent]
            if obj.edge in PORTABLE_EDGES:
                if (obj.kind not in ('item', 'box', 'bag', 'equipment', 'wearable') or
                        not obj.portable or obj.parent is None or
                        self.objects[obj.parent].kind != 'actor'):
                    raise ValueError('portable binding cannot exempt a building')

    def protected(self):
        self.validate()
        protected = {r for r, o in self.objects.items()
                     if o.kind == 'gestation' or (o.kind == 'actor' and o.alive)}
        frontier = []
        for r, o in self.objects.items():
            if (o.parent in protected and o.edge in PORTABLE_EDGES and
                    o.portable):
                protected.add(r)
                frontier.append(r)
        while frontier:
            parent = frontier.pop()
            for r, o in self.objects.items():
                if (o.parent == parent and o.edge in CONTENTS_EDGES and
                        o.portable and r not in protected):
                    protected.add(r)
                    frontier.append(r)
        return protected

    def plan(self, direct_hits):
        protected = self.protected()
        candidates = set(direct_hits)
        if not candidates <= set(self.objects):
            raise ValueError('unknown hit')
        frontier = list(candidates)
        while frontier:
            parent = frontier.pop()
            for r, o in self.objects.items():
                if o.parent == parent and r not in candidates:
                    candidates.add(r)
                    frontier.append(r)
        delete = {r for r in candidates - protected
                  if self.objects[r].kind not in ('actor', 'gestation')}
        return protected, delete

    def commit(self, command, direct_hits, expected_revision):
        if command in self.receipts or self.revision != expected_revision:
            return False
        protected, delete = self.plan(direct_hits)
        draft = deepcopy(self)
        for ref in sorted(delete):
            obj = draft.objects.pop(ref)
            draft.losses[(command, ref)] = obj.quantity
        for ref, obj in draft.objects.items():
            if obj.parent in delete:
                obj.parent = None
                obj.edge = 'NONE'
                if obj.kind == 'gestation' or (obj.kind == 'actor' and obj.alive):
                    draft.hazards.add(ref)
        assert not delete & protected
        draft.receipts.add(command)
        draft.revision += 1
        draft.validate()
        self.__dict__.update(draft.__dict__)
        return True


def fixture(alive=True, carried=True, inside=True):
    return Scene({
        1: Entity(1, 'building'),
        2: Entity(2, 'actor', 1 if inside else None,
                  'OCCUPANCY' if inside else 'NONE', alive=alive),
        3: Entity(3, 'bag', 2 if carried else 1,
                  'CARRIED' if carried else 'CONTAINED', portable=True),
        4: Entity(4, 'box', 3, 'CONTAINED', portable=True),
        5: Entity(5, 'item', 4, 'CONTAINED', portable=True,
                  quantity=4, owner=77, custody=2),
        6: Entity(6, 'tree'),
        7: Entity(7, 'item', 6, 'CONTAINED', portable=True, quantity=8),
        8: Entity(8, 'item', 1, 'CONTAINED', portable=True,
                  quantity=6, owner=2, custody=2),
        9: Entity(9, 'gestation', 1, 'CONTAINED'),
    })


def run():
    checks = []
    def check(name, ok):
        if not ok:
            raise AssertionError(name)
        checks.append(name)

    s = fixture()
    held = deepcopy({r: s.objects[r] for r in (3, 4, 5)})
    check('house_corner_wipe_keeps_nested_live_carry', s.commit('paint', {1}, 1))
    check('third_party_goods_keep_every_absolute_field',
          all(s.objects[r] == obj for r, obj in held.items()))
    check('ground_owner_claim_does_not_protect_stored_goods', 8 not in s.objects)
    check('living_pose_and_gestation_survive_deleted_parent',
          s.objects[2].pose == 42 and s.objects[9].pose == 42 and
          s.hazards == {2, 9})
    check('portable_refs_absent_from_loss_receipts',
          not {3, 4, 5} & {ref for unused, ref in s.losses})
    before = deepcopy(s)
    check('command_replay_no_loss_or_duplicate_gear',
          not s.commit('paint', {6}, s.revision) and s == before)
    check('stale_preview_no_partial_world_write',
          not s.commit('other', {6}, 1) and s == before)
    s = fixture()
    s.objects[5].source_tree = 6
    fruit = deepcopy(s.objects[5])
    s.commit('tree', {6}, 1)
    check('destroyed_source_tree_does_not_erase_harvested_carry',
          6 not in s.objects and 7 not in s.objects and s.objects[5] == fruit)
    s = fixture(carried=False)
    s.commit('drop', {1}, 1)
    check('dropped_bag_is_not_protected_by_owner_or_task', 3 not in s.objects and 5 not in s.objects)
    s = fixture(alive=False)
    s.commit('dead', {1}, 1)
    check('already_dead_actor_not_a_portable_protection_root', 3 not in s.objects and 5 not in s.objects)
    for edge in sorted(PORTABLE_EDGES):
        s = fixture()
        s.objects[3].edge = edge
        before = deepcopy(s.objects[3])
        s.commit('direct', {3, 5}, 1)
        check('direct_hit_keeps_actual_' + edge.lower(), s.objects[3] == before and 5 in s.objects)
    s = fixture()
    s.objects[1].parent, s.objects[1].edge = 2, 'CARRIED'
    before = deepcopy(s)
    try:
        s.commit('forged', {1}, 1)
    except ValueError:
        rejected = True
    else:
        rejected = False
    check('invalid_or_cyclic_fake_carry_rejected_without_write', rejected and s == before)
    s = fixture(inside=False)
    s.objects[1].parent, s.objects[1].edge = 2, 'CARRIED'
    s.objects[1].portable = True
    before = deepcopy(s)
    try:
        s.commit('false-building-binding', {1}, 1)
    except ValueError:
        rejected = True
    else:
        rejected = False
    check('building_cannot_be_reclassified_as_a_carried_item', rejected and s == before)
    # Abstract delivery/chamber state: preserving a carried batch does not
    # refresh its lifetime, reload progress, ammunition or random stream.
    s = fixture()
    before = deepcopy(s.objects[5])
    s.commit('receiver', {1}, 1)
    check('destroyed_receiver_no_reset_of_carried_weapon_or_food', s.objects[5] == before)

    worlds = 0
    for alive, carried, inside in product((False, True), repeat=3):
        for flags in product((False, True), repeat=9):
            s = fixture(alive, carried, inside)
            direct = {i + 1 for i, on in enumerate(flags) if on}
            old = deepcopy(s)
            # Independent ancestor-walk oracle; no reuse of plan's traversal.
            protected = {9} | ({2} if alive else set())
            if alive and carried:
                protected |= {3, 4, 5}
            expected = set()
            for ref, obj in old.objects.items():
                if ref in protected or obj.kind in ('actor', 'gestation'):
                    continue
                ancestor = ref
                while ancestor is not None:
                    if ancestor in direct:
                        expected.add(ref)
                        break
                    ancestor = old.objects[ancestor].parent
            s.commit('finite', direct, 1)
            assert set(old.objects) - set(s.objects) == expected
            assert not expected & protected
            assert sum(s.losses.values()) == sum(old.objects[r].quantity for r in expected)
            for ref in protected & {3, 4, 5}:
                assert s.objects[ref] == old.objects[ref]
            worlds += 1
    check('4096_parent_hit_and_life_combinations_match_independent_oracle', worlds == 4096)
    orders = 0
    for order in permutations(range(1, 10)):
        # First 720 of 9! orders permute the six remaining object entries.
        if orders == 720:
            break
        original = fixture()
        reordered = Scene({ref: deepcopy(original.objects[ref]) for ref in order})
        check_set = original.plan({1})
        assert reordered.plan({1}) == check_set
        orders += 1
    check('720_dictionary_orders_do_not_change_protection_or_loss', orders == 720)
    return {'passed': True, 'scope': 'finite_attachment_and_ancestor_closure_only',
            'production_implemented': False, 'named_check_count': len(checks),
            'named_checks': checks, 'exhaustive_worlds': worlds,
            'insertion_orders': orders,
            'not_tested': ['3D_geometry_and_collision', 'real_inventory_runtime',
                           'thermal_death_and_estate_runtime', 'save_fault_injection', 'GPU_Steam']}

if __name__ == '__main__':
    print(json.dumps(run(), ensure_ascii=False, indent=2))
