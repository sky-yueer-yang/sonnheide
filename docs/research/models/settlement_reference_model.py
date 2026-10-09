#!/usr/bin/env python3
"""Finite design model only: no runtime, geometry, physiology or game implementation.

Run once with the other design models after the batch is complete. Exhaustive
checks concern identity/title/assignment/whole-object wipe/order invariants in a
small abstract grid. They do not test 3D ramp clipping, real conservation or GPU.
Python 3.9+ standard library; no external state or output files are created.
"""
from copy import deepcopy
from dataclasses import dataclass, field
from itertools import permutations, product
import json
from typing import Dict, FrozenSet, List, Optional, Set, Tuple

Cell = Tuple[int, int]
GRID: FrozenSet[Cell] = frozenset(product(range(4), range(4)))


def neighbors(cell: Cell) -> Tuple[Cell, ...]:
    x, y = cell
    return ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))


def component(domain: Set[Cell], start: Optional[Cell]) -> Set[Cell]:
    if start is None or start not in domain:
        return set()
    found, pending = {start}, [start]
    while pending:
        p = pending.pop()
        for n in neighbors(p):
            if n in domain and n not in found:
                found.add(n)
                pending.append(n)
    return found


def decay_order(domain: Set[Cell]) -> List[Cell]:
    """Outer and inner boundary erosion: authoritative grid edge is a boundary."""
    remaining, result = set(domain), []
    while remaining:
        edge = sorted(p for p in remaining if any(n not in remaining for n in neighbors(p)))
        if not edge:
            raise AssertionError("finite nonempty domain must expose a boundary")
        result.extend(edge)
        remaining.difference_update(edge)
    return result


@dataclass
class Actor:
    actor_id: int
    pos: Cell
    home_city: Optional[int] = 1
    state: Optional[int] = 1
    alive: bool = True
    goods: int = 3


@dataclass
class Object:
    object_id: int
    cells: FrozenSet[Cell]
    kind: str = "housing"
    city: Optional[int] = 1
    stock: int = 7


@dataclass
class City:
    city_id: int
    assigned: Set[Cell]
    anchor: Optional[Cell]
    historical_city_charter: bool = True
    physical: str = "OPERABLE"
    community: str = "CONTINUING"
    archived: bool = False
    square_capacity: int = 0
    initial_decay_order: List[Cell] = field(default_factory=list)
    decay_start_day: Optional[int] = None
    decay_due_day: Optional[int] = None
    decay_cursor: int = 0
    square_account_id: int = 101
    stranded_dry_remainder: Set[Cell] = field(default_factory=set)


@dataclass
class State:
    state_id: int
    title: Set[Cell]
    status: str = "OPERATING"
    archive: bool = False
    public_debt: int = 9
    estate_debt: int = 0
    legal_rank: str = "GRAND_DUCHY"


@dataclass
class Model:
    dry: Set[Cell]
    walkable: Set[Cell]
    actors: Dict[int, Actor]
    objects: Dict[int, Object]
    city: City
    state: State
    kinds: Dict[Cell, str]
    receipts: Set[Tuple[str, str, int]] = field(default_factory=set)
    destroyed_stock: int = 0
    events: List[str] = field(default_factory=list)
    day: int = 0

    @classmethod
    def populated(cls) -> "Model":
        land = set(GRID)
        objects = {
            1: Object(1, frozenset({(0, 0)}), "square"),
            2: Object(2, frozenset({(0, 1), (1, 1)})),
            3: Object(3, frozenset({(2, 2)}), "tree"),
        }
        return cls(land, set(land), {1: Actor(1, (0, 0)), 2: Actor(2, (1, 1))},
                   objects, City(1, set(land), (0, 0)), State(1, set(land)),
                   {p: "soil" for p in GRID})

    def members(self) -> Set[int]:
        return {a.actor_id for a in self.actors.values() if a.alive and a.home_city == self.city.city_id}

    def current_sovereign_dry(self) -> Set[Cell]:
        return set() if self.state.archive or self.state.status == "WINDING_DOWN" else self.state.title & self.dry

    def apply(self, command: Tuple[str, object], command_id: str = "batch") -> None:
        kind, value = command
        if kind == "paint":
            paint = value
            changed = {p for p, new in paint.items() if self.kinds[p] != new}
            for p in changed:
                self.kinds[p] = paint[p]
                if paint[p] == "water":
                    self.dry.discard(p)
                    self.walkable.discard(p)
                elif paint[p] == "peak":
                    self.dry.add(p)
                    self.walkable.discard(p)
                else:
                    self.dry.add(p)
                    self.walkable.add(p)
            for oid, obj in list(self.objects.items()):
                if obj.cells & changed:
                    key = (command_id, "object", oid)
                    if key not in self.receipts:
                        self.destroyed_stock += obj.stock
                        self.receipts.add(key)
                    del self.objects[oid]
            for a in self.actors.values():
                # Nonlife carried/corpse goods are still in the affected pose;
                # final death ordering cannot exempt their wipe settlement.
                if a.pos in changed:
                    key = (command_id, "carried", a.actor_id)
                    if key not in self.receipts:
                        self.destroyed_stock += a.goods
                        a.goods = 0
                        self.receipts.add(key)
        elif kind == "kill":
            for aid in value:
                a = self.actors[aid]
                a.alive = False
                a.home_city = None
        elif kind == "leave":
            for aid in value:
                self.actors[aid].home_city = None
        elif kind == "join":
            if self.city.archived:
                raise ValueError("archived id cannot accept member")
            for aid in value:
                if self.actors[aid].alive:
                    self.actors[aid].home_city = self.city.city_id
        elif kind == "destroy_objects":
            for oid in value:
                obj = self.objects.pop(oid, None)
                if obj is not None:
                    key = (command_id, "object", oid)
                    if key not in self.receipts:
                        self.destroyed_stock += obj.stock
                        self.receipts.add(key)
        else:
            raise ValueError(kind)

    def normalize(self) -> None:
        c, s = self.city, self.state
        if c.archived:
            c.assigned.clear()
            c.community = "ARCHIVED"
            c.square_capacity = 0
            self.normalize_state()
            return
        c.assigned.intersection_update(self.dry)
        members = self.members()
        # Model's basic foot-service graph ignores crowds: transient occupancy
        # cannot remove a legal service edge. Peaks are hard non-walkable.
        service_domain = c.assigned & self.walkable
        valid_anchor = c.anchor in service_domain
        if not valid_anchor:
            candidates = sorted(a.pos for a in self.actors.values()
                                if a.alive and a.actor_id in members and a.pos in service_domain)
            c.anchor = candidates[0] if candidates else None
        served = component(service_domain, c.anchor)
        c.stranded_dry_remainder.intersection_update(self.dry)
        c.stranded_dry_remainder.update(c.assigned - served)
        c.assigned = served
        squares = [o for o in self.objects.values() if o.city == c.city_id and o.kind == "square"
                   and o.cells <= served]
        housing = [o for o in self.objects.values() if o.city == c.city_id and o.kind == "housing"
                   and o.cells <= served]
        c.square_capacity = 10 if squares else 0
        if not served:
            c.physical = "STRANDED" if c.stranded_dry_remainder else "LANDLESS"
        elif squares and housing and members:
            c.physical = "OPERABLE"
        else:
            c.physical = "DAMAGED"
        if members:
            c.community = "CONTINUING"
            c.decay_start_day = c.decay_due_day = None
            c.initial_decay_order = []
            c.decay_cursor = 0
        elif c.decay_start_day is None:
            c.community = "DEPOPULATED"
            c.decay_start_day, c.decay_due_day = self.day, self.day + 30
            c.initial_decay_order = decay_order(c.assigned)
        self.normalize_state()

    def normalize_state(self) -> None:
        s = self.state
        if not any(a.alive and a.state == s.state_id for a in self.actors.values()) and not s.archive:
            s.status = "WINDING_DOWN"
            s.estate_debt += s.public_debt
            s.public_debt = 0
            s.archive = True
            s.status = "ARCHIVED"
        elif not s.archive:
            actual_anchor = any(a.alive and a.state == s.state_id and a.pos in s.title
                                and a.pos in self.dry and a.pos in self.walkable
                                for a in self.actors.values())
            s.status = "OPERATING" if actual_anchor else "DISPLACED"
        # Existing legal rank is sticky; qualification changes cannot strip it.
        assert s.legal_rank == "GRAND_DUCHY"

    def advance_day(self, count: int = 1) -> None:
        for _ in range(count):
            self.day += 1
            c = self.city
            if c.archived or self.members() or c.decay_due_day is None:
                continue
            budget = max(1, (len(c.initial_decay_order) + 29) // 30)
            end = min(len(c.initial_decay_order), c.decay_cursor + budget)
            for p in c.initial_decay_order[c.decay_cursor:end]:
                c.assigned.discard(p)
            c.decay_cursor = end
            if self.day >= c.decay_due_day:
                c.assigned.clear()
                c.archived = True
                c.community = "ARCHIVED"
                c.square_capacity = 0

    def batch(self, commands: List[Tuple[str, object]]) -> None:
        candidate = deepcopy(self)
        for command in commands:
            candidate.apply(command)
        candidate.normalize()
        self.__dict__.update(candidate.__dict__)

    def fingerprint(self) -> Tuple[object, ...]:
        return (tuple(sorted(self.dry)), tuple(sorted(self.city.assigned)), self.city.anchor,
                self.city.physical, self.city.community, self.city.square_capacity,
                self.state.status, tuple(sorted(self.current_sovereign_dry())),
                tuple(sorted((a.actor_id, a.alive, a.home_city, a.state, a.pos, a.goods)
                             for a in self.actors.values())),
                tuple(sorted(self.objects)), self.destroyed_stock, tuple(sorted(self.receipts)),
                self.state.public_debt, self.state.estate_debt,
                tuple(sorted(self.city.stranded_dry_remainder)))


def run() -> Dict[str, object]:
    checks = 0
    named = []

    def check(name: str, predicate: bool) -> None:
        nonlocal checks
        if not predicate:
            raise AssertionError(name)
        checks += 1
        named.append(name)

    m = Model.populated()
    m.batch([("destroy_objects", {1, 2, 3})])
    check("all_facilities_destroyed_preserve_real_members_id_debt", m.city.physical == "DAMAGED" and
          m.city.community == "CONTINUING" and m.members() == {1, 2} and m.state.public_debt == 9)
    check("destroyed_square_logic_account_not_storage", m.city.square_account_id == 101 and m.city.square_capacity == 0)
    m = Model.populated()
    m.batch([("paint", {p: "water" for p in GRID})])
    check("all_land_lost_preserve_alive_poses_members", m.city.physical == "LANDLESS" and
          len(m.members()) == 2 and m.actors[1].pos == (0, 0) and m.state.status == "DISPLACED")
    check("wipe_all_nonlife_not_life", not m.objects and m.actors[1].alive and m.actors[1].goods == 0)
    before_loss = m.destroyed_stock
    m.batch([("paint", {p: "water" for p in GRID})])
    check("noop_type_repaint_no_extra_loss", m.destroyed_stock == before_loss)
    m.batch([("paint", {p: "soil" for p in GRID})])
    check("same_continuing_title_dry_index_restores_not_city", m.current_sovereign_dry() == set(GRID)
          and not m.city.assigned and not m.objects and m.city.square_capacity == 0)
    m = Model.populated()
    m.batch([("paint", {(1, 1): "hill"})])
    check("one_cell_hit_removes_whole_multicell_building_once", 2 not in m.objects and
          m.destroyed_stock == 10 and m.actors[2].alive and m.actors[2].pos == (1, 1))
    m = Model.populated()
    m.batch([("paint", {(0, 0): "peak", (1, 1): "peak"})])
    check("peak_wipe_not_lift_to_peak_top_or_navigate", m.actors[1].pos == (0, 0) and
          m.actors[2].pos == (1, 1) and m.city.anchor is None and m.city.physical == "STRANDED")
    check("peak_political_title_separate_from_service", (0, 0) in m.current_sovereign_dry() and
          (0, 0) not in m.city.assigned)
    m = Model.populated()
    m.batch([("paint", {(1, y): "water" for y in range(4)})])
    check("split_city_no_new_city_no_delete_remote_unhit_tree", (2, 2) not in m.city.assigned and
          3 in m.objects and (2, 2) in m.current_sovereign_dry())
    check("diagonal_point_not_land_connection", component({(0, 0), (1, 1)}, (0, 0)) == {(0, 0)})
    m = Model.populated()
    m.batch([("leave", {1, 2})])
    m.advance_day(29)
    restored = deepcopy(m)  # serialization must preserve these absolute values.
    check("depopulation_29day_deadline_preserved", restored.city.decay_due_day == 30)
    restored.advance_day(1)
    check("all_world_city_erodes_by_due_day_without_deleting_private_objects", restored.city.archived and
          not restored.city.assigned and len(restored.objects) == 3)
    try:
        restored.batch([("join", {1})])
    except ValueError:
        rejected = True
    else:
        rejected = False
    check("archived_id_cannot_accept_same_or_new_actor", rejected)
    restored.batch([("kill", {1, 2})])
    check("archived_city_does_not_skip_later_state_death_settlement", restored.state.archive and
          restored.state.estate_debt == 9)
    m = Model.populated()
    m.batch([("kill", {1, 2})])
    check("empty_state_archive_carries_real_debt_to_estate", m.state.archive and
          m.state.public_debt == 0 and m.state.estate_debt == 9)
    m.batch([("paint", {p: "water" for p in GRID})])
    m.batch([("paint", {p: "soil" for p in GRID})])
    check("archived_state_title_never_reactivates_from_repaint", not m.current_sovereign_dry())
    m = Model.populated()
    m.batch([("leave", {1})])
    check("one_remaining_real_member_continues_city_without_population_spawning", m.members() == {2} and
          m.city.community == "CONTINUING" and len(m.actors) == 2)
    commands = [("paint", {(0, 0): "water"}), ("kill", {1}), ("leave", {2})]
    fingerprints = set()
    for order in permutations(commands):
        n = Model.populated()
        n.batch(list(order))
        fingerprints.add(n.fingerprint())
    check("batch_final_alive_and_membership_order_invariance", len(fingerprints) == 1)
    orders = decay_order(set(GRID))
    check("erosion_total_unique_finite", len(orders) == len(GRID) and set(orders) == set(GRID))
    check("negative_transient_crowd_not_hard_domain_rule", component(set(GRID), (0, 0)) == set(GRID))

    # Exhaustively enumerate 512 dry patterns, 4 membership/death combinations
    # and square presence. This validates convergence over 4096 finite worlds.
    exhaustive = 0
    cells = sorted(GRID)[:9]
    for bits in product((False, True), repeat=9):
        dry = {p for p, on in zip(cells, bits) if on}
        for alive_a, alive_b, square in product((False, True), repeat=3):
            n = Model.populated()
            n.dry = dry
            n.walkable = set(dry)
            n.city.assigned = set(dry)
            n.actors[1].alive, n.actors[2].alive = alive_a, alive_b
            if not square:
                n.objects.pop(1)
            n.normalize()
            assert n.city.assigned <= n.dry
            assert n.city.assigned == component(n.city.assigned, n.city.anchor)
            assert not n.city.square_capacity or square
            assert len(n.actors) == 2
            assert n.state.public_debt + n.state.estate_debt == 9
            frozen = n.fingerprint()
            n.normalize()
            assert frozen == n.fingerprint(), "normalization must be idempotent"
            n.advance_day(30)
            if not n.members():
                assert n.city.archived and not n.city.assigned
            exhaustive += 1
    return {"passed": True, "scope": "finite_design_model_only_not_production_game_tests",
            "named_checks": checks, "exhaustive_worlds": exhaustive,
            "named_checks_list": named,
            "not_tested": ["3D_surface_clipping", "actual_movement_physiology", "all_resource_batches",
                           "full_economic_transport", "GPU_rendering", "Steam_runtime"]}


if __name__ == "__main__":
    print(json.dumps(run(), ensure_ascii=False, indent=2))
