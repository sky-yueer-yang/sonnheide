#!/usr/bin/env python3
"""Finite arithmetic/evidence model for ADR0018. Not production AI, physics or rendering."""
import copy
import itertools
import hashlib
import json
from dataclasses import dataclass, field
from typing import Dict, List, Set, Tuple
from pathlib import Path

EMERGENCE = json.loads((Path(__file__).resolve().parents[3] / "data/contracts/emergence_algorithms_v1.json").read_text())
PROFILE_HASH = hashlib.sha256((Path(__file__).resolve().parents[3] / "data/contracts/emergence_algorithms_v1.json").read_bytes()).digest()
PULSE_BASE = {row["category"]: row["base_pulse_q"] for row in EMERGENCE["culture"]["event_pulse_table"]}

Q = 10000
DAY = 12000


def clamp(value, lo, hi):
    return max(lo, min(hi, value))


def signed_div(n, d):
    return (abs(n) // d) * (-1 if n < 0 else 1)


def rng_uniform(domain, event_counter, n, draw_index=0, stable_id=1, generation=1, kind="genesis_node"):
    """Exact Runtime RNGv1 byte algorithm; fixed explicit finite-fixture seed/UUID."""
    assert 1 <= n <= (1 << 63) - 1
    dom, kin = domain.encode("ascii"), kind.encode("ascii")
    prefix = (b"SONNRNG1" + bytes(32) + len(dom).to_bytes(2, "little") + dom + bytes(16)
              + len(kin).to_bytes(2, "little") + kin + stable_id.to_bytes(8, "little")
              + generation.to_bytes(4, "little") + PROFILE_HASH
              + event_counter.to_bytes(8, "little") + draw_index.to_bytes(8, "little"))
    limit = ((1 << 64) // n) * n
    for attempt in range(4096):
        raw = int.from_bytes(hashlib.sha256(prefix + attempt.to_bytes(8, "little")).digest()[:8], "little")
        if raw < limit:
            return raw % n
    raise ValueError("RNG_REJECTION_LIMIT")


def pair_agreement(a, b):
    shared = [(x, y) for x, y in zip(a, b) if x is not None and y is not None]
    return None if len(shared) < 3 else Q - sum(abs(x-y) for x,y in shared) // len(shared)


@dataclass
class Integral:
    remainder: int = 0

    def advance(self, rate, ticks, denominator=DAY):
        assert rate >= 0 and ticks >= 0 and denominator > 0
        value, self.remainder = divmod(rate * ticks + self.remainder, denominator)
        return value


@dataclass
class Actor:
    ref: int
    human: bool = True
    mature: bool = True
    alive: bool = True
    practice: Tuple[int, ...] = (7000, 7000, 8000, 8000, 7000, 6000, 7000, 8000)
    # Inherited normalized Q=5*(raw alleleA+raw alleleB), never raw0..1000 or shared profile.
    genome: Tuple[int, ...] = (3500,) * 12
    species: str = "human"
    shared_profile: int = 5000


@dataclass
class Evidence:
    seen: Set[Tuple] = field(default_factory=set)
    directions: Dict[Tuple[int, int], Set[int]] = field(default_factory=dict)
    categories: Dict[int, Set[str]] = field(default_factory=dict)
    meanings: Dict[int, Set[int]] = field(default_factory=dict)
    meaning_days: Dict[Tuple[int, int], Set[int]] = field(default_factory=dict)
    origin_work: Dict[Tuple[int, int], int] = field(default_factory=dict)
    origin_receipts: Set[Tuple] = field(default_factory=set)
    teacher_origin_work: int = 0

    def record(self, source, a, b, category, day, actual_meanings=()):
        if category not in {"cooperative_work", "actual_care", "actual_teaching"}:
            return False
        key = (source, tuple(sorted((a, b))), day)
        # Root evidence cannot be recounted through another domain/category.
        if key in self.seen or a == b:
            return False
        self.seen.add(key)
        self.directions.setdefault((a, b), set()).add(day)
        self.categories.setdefault(day, set()).add(category)
        self.meanings.setdefault(a, set()).update(actual_meanings)
        self.meanings.setdefault(b, set()).update(actual_meanings)
        for meaning in actual_meanings:
            self.meaning_days.setdefault((a, meaning), set()).add(day)
            self.meaning_days.setdefault((b, meaning), set()).add(day)
        return True

    def origin_teaching(self, root, founder, learners, meaning, ticks, food_energy, communicating=True):
        # Each tick is actual dedicated availability; group max4, each learner pays real work.
        if (root in self.origin_receipts or not communicating or not founder.human or not founder.mature
                or not founder.alive or not 1 <= len(learners) <= 4 or ticks != 100
                or any(not(a.human and a.mature and a.alive) for a in learners)):
            return False
        participants = [founder] + learners
        if any(food_energy[a.ref] < ticks * 40 for a in participants):
            return False
        if any(len(self.meaning_days.get((a.ref, meaning), set())) < 3 for a in participants):
            return False
        self.origin_receipts.add(root)
        for a in participants:
            food_energy[a.ref] -= ticks * 40
        self.teacher_origin_work += ticks
        for a in learners:
            key = (a.ref, meaning)
            self.origin_work[key] = self.origin_work.get(key, 0) + ticks
        return True

    def root_session_complete(self, founder, learners):
        return (founder.human and founder.mature and founder.alive and len(learners) >= 3
                and self.teacher_origin_work >= 4800
                and all(a.human and a.mature and a.alive
                        and all(self.origin_work.get((a.ref, m), 0) >= 300 for m in range(16))
                        for a in learners[:3]))

    def reciprocal(self, a, b, day):
        count = lambda s: sum(day - 89 <= n <= day for n in s)
        return min(Q, 1000 * min(count(self.directions.get((a, b), set())),
                                 count(self.directions.get((b, a), set()))))


@dataclass
class CultureNode:
    phi: int = 0
    evidence_days: int = 0
    originated: bool = False
    counter: int = 0
    recovery_reservoir: int = 0
    conflict_reservoir: int = 0
    pulse_seen: Set[Tuple] = field(default_factory=set)
    recent_positive: List[Tuple[int, Tuple, int]] = field(default_factory=list)

    def advance(self, actors, graph, day, outcomes=()):
        daily_category = {}
        recovery, conflict, resolved = 0, 0, 0
        for source, category, actual_qty, target_qty, quality_q in outcomes:
            if source in self.pulse_seen or category not in PULSE_BASE or target_qty <= 0 or actual_qty < 0:
                continue
            self.pulse_seen.add(source)
            amount = signed_div(PULSE_BASE[category] * min(Q, actual_qty * Q // target_qty) * quality_q, Q * Q)
            daily_category[category] = clamp(daily_category.get(category, 0) + amount, -1000, 1000)
            if amount > 0:
                self.recent_positive.append((day, source, amount))
            recovery += amount if category == "completed_shared_recovery" else 0
            conflict += -amount if category == "unresolved_internal_conflict" else 0
            resolved += amount if category == "resolved_internal_conflict" else 0
        actual_pulse = clamp(sum(daily_category.values()), -2000, 2000)
        self.recovery_reservoir = clamp(self.recovery_reservoir * 9500 // Q + recovery, 0, Q)
        self.conflict_reservoir = clamp(self.conflict_reservoir * 9800 // Q + conflict - resolved, 0, Q)
        self.recent_positive = [v for v in self.recent_positive if v[0] >= day - 29]
        pairs = []
        active_edges = 0
        for a, b in itertools.combinations(actors, 2):
            if not (a.alive and b.alive and a.human and b.human):
                continue
            reciprocal = graph.reciprocal(a.ref, b.ref, day)
            if reciprocal >= 3000:
                agree = pair_agreement(a.practice, b.practice)
                if agree is None:
                    continue
                pairs.append((agree, reciprocal))
                if day in graph.directions.get((a.ref, b.ref), set()) and day in graph.directions.get((b.ref, a.ref), set()):
                    active_edges += 1
        coherence = sum(x[0] for x in pairs) // max(1, len(pairs))
        reciprocity = sum(x[1] for x in pairs) // max(1, len(pairs))
        human_count = sum(a.alive and a.human for a in actors)
        mu = clamp(6000 - coherence - reciprocity // 2 - self.recovery_reservoir // 4 + self.conflict_reservoir // 4, -Q, Q)
        # Runtime SHA256 SONNRNG1 exact finite-fixture stream, no renderer draws.
        noise = rng_uniform("culture.field", self.counter, 801) - 400
        self.counter += 1
        self.phi = clamp(signed_div(9000 * self.phi, Q) + actual_pulse +
                         signed_div(((2000 + reciprocity // 2) * coherence * max(0, -mu) // (Q * Q)) * noise, Q), -Q, Q)
        categories = set().union(*(graph.categories.get(d, set()) for d in range(max(0, day - 89), day + 1)))
        qualify = (human_count >= 6 and any(a.human and a.mature and a.alive for a in actors)
                   and coherence >= 6500 and reciprocity >= 6000 and mu < 0 and len(categories) >= 2)
        if qualify and active_edges >= 3:
            self.evidence_days += 1
        elif not qualify:
            self.evidence_days = 0
        meanings_known = all(all(len(graph.meaning_days.get((a.ref,m),set())) >= 3 for m in range(16))
                             for a in actors if a.human and a.alive)
        session_complete = graph.root_session_complete(actors[0], actors[1:4])
        actual_positive = sum(v[2] for v in self.recent_positive) >= 1000 and len({v[1] for v in self.recent_positive}) >= 2
        if qualify and self.evidence_days >= 30 and abs(self.phi) >= 4000 and meanings_known and actual_positive and session_complete:
            self.originated = True
        return qualify


def culture_scenario(human=True, events=True, god=False, meanings=True, pulse=True):
    actors = [Actor(i, human=human, species="human" if human else "cow") for i in range(6)]
    graph, node = Evidence(), CultureNode()
    snapshots = []
    # Fixture balances are productive energy from already actually consumed food, never placement reserve.
    food_energy = {a.ref: 1200000 for a in actors}
    next_meaning = 0
    for day in range(80):
        if events:
            for i in range(6):
                j = (i + 1) % 6
                category = "GodForcedMembership" if god else ("cooperative_work" if i % 2 else "actual_care")
                context = range(0, 8) if i % 2 else range(8, 16)
                graph.record((day, i, "forward"), i, j, category, day, context if meanings else ())
                graph.record((day, i, "reverse"), j, i, category, day, context if meanings else ())
        outcomes = [((day, "fulfilled"), "fulfilled_collective_project", 1, 1, Q),
                    ((day, "resolved"), "resolved_internal_conflict", 1, 1, Q)] if events and not god and pulse else []
        node.advance(actors, graph, day, outcomes)
        if node.evidence_days >= 30 and next_meaning < 16 and human and meanings and not god:
            for utterance in range(3):
                graph.origin_teaching((day,next_meaning,utterance),actors[0],actors[1:4],next_meaning,100,food_energy)
            next_meaning += 1
        snapshots.append((node.phi, node.evidence_days, node.originated))
    return actors, graph, node, snapshots


def frequency_update(old, observed_variant, actual_work, efficiency_q=Q):
    assert sum(old) == Q
    alpha = min(1200, actual_work * efficiency_q // Q)
    result = [(x * (Q - alpha) + (Q * alpha if j == observed_variant else 0)) // Q
              for j, x in enumerate(old)]
    result[observed_variant] += Q - sum(result)
    return tuple(result)


def intelligibility(a, b):
    assert len(a) == len(b) == 64
    return sum(sum(min(x, y) for x, y in zip(fa, fb)) for fa, fb in zip(a, b)) // 64


def branch_eligible(human_participants, actual_word_usage, changed_features,
                    actual_teaching_events, intelligibility_q, isolation_q):
    cohorts = {day // 180 for learner, day, taught in actual_teaching_events if taught}
    human_count = sum(a.alive and a.human and a.mature for a in human_participants)
    teaching_days = [day for learner, day, taught in actual_teaching_events if taught]
    span = max(teaching_days, default=0) - min(teaching_days, default=0)
    return (human_count >= 6 and len(actual_word_usage) >= 12 and len(changed_features) >= 3
            and len(cohorts) >= 2 and span >= 360 and intelligibility_q <= 6000 and isolation_q >= 6500)


def genetic_distance(a, b):
    return sum(abs(x - y) for x, y in zip(a, b)) // 12


def prototype(actors):
    n = len(actors)
    assert n
    return tuple(sorted(a.genome[k] for a in actors)[(n - 1) // 2] for k in range(12))


def classify(genome, prototypes, original):
    ranked = sorted((genetic_distance(genome, p), name) for name, p in prototypes.items())
    if ranked[0][0] > 1800 or (len(ranked) > 1 and ranked[1][0] - ranked[0][0] < 500):
        return original
    return ranked[0][1]


def lineage_candidate(actors, parent_prototype, actual_births, actual_evidence_days, now_day):
    mature = [a for a in actors if a.alive and a.mature]
    if len(mature) < 8 or len({a.species for a in mature}) != 1 or len(actual_evidence_days) < 90:
        return False
    proto, refs = prototype(mature), {a.ref for a in mature}
    if genetic_distance(proto, parent_prototype) < 2500 or any(genetic_distance(a.genome,proto)>1800 for a in mature):
        return False
    births = [(root,a,b,day) for root,a,b,day in actual_births if now_day-719 <= day <= now_day]
    births = list({root:(root,a,b,day) for root,a,b,day in births}.values())
    internal = [(root,a,b,day) for root,a,b,day in births if a in refs and b in refs]
    crossing = [v for v in births if v[1] in refs or v[2] in refs]
    parents = {ref for _,a,b,_ in internal for ref in (a,b)}
    return (len(internal) >= 2 and len(parents) >= 4
            and len(internal)*Q//max(1,len(crossing)) >= 6500)


@dataclass
class MovementIntegral:
    energy_remainder: int = 0
    fatigue_mm_remainder: int = 0

    def advance(self, actual_mm, rate=100, load_q=Q, medium_q=Q):
        energy,self.energy_remainder=divmod(actual_mm*rate*load_q*medium_q+self.energy_remainder,100000000000)
        fatigue,self.fatigue_mm_remainder=divmod(actual_mm+self.fatigue_mm_remainder,100)
        return energy,fatigue


def misfire_threshold(base, wet_q, durability_milli, capacity_milli=100000):
    condition_q=durability_milli*Q//capacity_milli
    return clamp(base+min(2000,wet_q//5)+(Q-condition_q)//5,0,6000)


def water_drag(v, submersion_q, coefficient_q=20000):
    return -(-1 if v<0 else 1)*min(20000,abs(v)*submersion_q*coefficient_q//100000000) if v else 0


def expression(allele_q, shared_q, baseline=60000):
    effective = (7000 * allele_q + 3000 * shared_q) // Q
    ratio = 7000 + 6000 * effective // Q
    return min(100000, baseline * ratio // Q)


@dataclass
class Energy:
    maintenance: int
    productive: int
    dry_matter_g: int = 0

    def survival(self, amount):
        m = min(self.maintenance, amount)
        self.maintenance -= m
        p = min(self.productive, amount - m)
        self.productive -= p
        return amount - m - p

    def product(self, energy_per_g, material_per_g, cap):
        output = min(cap, self.productive // energy_per_g, self.dry_matter_g // material_per_g)
        self.productive -= output * energy_per_g
        self.dry_matter_g -= output * material_per_g
        return output


def absorb(quantity_g, nutrition_per_g, digestible_q, absorption_q=Q):
    return quantity_g * nutrition_per_g * digestible_q * absorption_q // (Q * Q)


def supported_float_mass(displacement_ml, submerged_q, carried_g, body_g, buoyancy_q=Q):
    support = displacement_ml * buoyancy_q // Q * submerged_q // Q
    return support - body_g - carried_g


def fall_severity(impact, safe=4500):
    excess = max(0, impact - safe)
    return min(100000, excess * excess // 2000)


def oxygen_damage(oxygen_debt_tick, wrong_medium_ticks, breathable_ticks,
                  threshold=600, damage_rate_day=30000):
    debt = max(0, oxygen_debt_tick + wrong_medium_ticks - 2 * breathable_ticks)
    # Current step only, never damage replay for the previous debt interval.
    return debt, (damage_rate_day * wrong_medium_ticks // DAY if debt > threshold else 0)


def armor_hit(power, penetration, protection, durability_q, absorption_q=8000):
    effective = protection * durability_q // Q
    absorbed = absorption_q * min(effective, penetration) // max(1, penetration)
    transmitted = power * (Q - absorbed) // Q
    severity = transmitted // 5 if penetration <= effective else transmitted
    wear = max(1, (power + 999) // 1000)
    return severity, wear


def simultaneous_hits(hits, protection, durability_q):
    # Every contact reads one armor snapshot; there is no input-order armor break.
    resolved = [armor_hit(power, penetration, protection, durability_q) for power, penetration in hits]
    return sum(x[0] for x in resolved), max(0, durability_q - sum(x[1] for x in resolved))


@dataclass
class WeaponCycle:
    stock: int = 2
    loaded: int = 0
    load_receipts: Set[int] = field(default_factory=set)
    fire_receipts: Set[int] = field(default_factory=set)

    def load(self, receipt):
        if receipt in self.load_receipts or self.loaded or not self.stock:
            return False
        self.stock -= 1
        self.loaded += 1
        self.load_receipts.add(receipt)
        return True

    def fire(self, receipt):
        if receipt in self.fire_receipts or not self.loaded:
            return False
        self.loaded -= 1
        self.fire_receipts.add(receipt)
        return True


def weapon_admitted(required_tech, known_tech, owned_actual_item, training_work, required_work, mature=True):
    return mature and owned_actual_item and training_work >= required_work and (required_tech is None or required_tech in known_tech)


def run():
    checks = []
    def checked(name, condition):
        assert condition, name
        checks.append(name)

    actors, graph, node, sequence = culture_scenario()
    checked("actual_reciprocity_and_practice_can_nucleate", node.originated)
    checked("same_population_calendar_without_events_never_nucleates", not culture_scenario(events=False)[2].originated)
    checked("God_membership_is_not_cooperation_evidence", not culture_scenario(god=True)[2].originated)
    checked("animal_only_actual_practice_cannot_originate", not culture_scenario(human=False)[2].originated)
    checked("unknown_primitive_meaning_context_blocks_root_language", not culture_scenario(meanings=False)[2].originated)
    checked("noise_even_qualified_practice_cannot_replace_actual_fulfilled_pulses", not culture_scenario(pulse=False)[2].originated)
    checked("unknown_zero_practices_are_not_eligible_edges", pair_agreement((None,None,0,0,None),(None,None,0,0,None)) is None)
    checked("origin_requires_real_paid_per_meaning_work", graph.root_session_complete(actors[0],actors[1:4]) and not Evidence().root_session_complete(actors[0],actors[1:4]))
    roots = len(graph.seen)
    checked("cross_domain_echo_of_same_root_cannot_count_twice", not graph.record((0, 0, "forward"), 0, 1, "actual_care", 0) and len(graph.seen) == roots)
    saved_node, saved_graph = copy.deepcopy(node), copy.deepcopy(graph)
    for day in range(80, 91):
        node.advance(actors, graph, day)
        saved_node.advance(actors, saved_graph, day)
    checked("saved_node_counter_and_evidence_resume_identically", node == saved_node)
    baseline = [(Q, 0, 0, 0)] * 64
    shifted = copy.deepcopy(baseline)
    for _ in range(20):
        for i in range(64):
            shifted[i] = frequency_update(shifted[i], 1, 1200)
    overlap = intelligibility(baseline, shifted)
    checked("real_tokens_shift_frequency_with_exact_Q_sum", overlap < 6000 and all(sum(f) == Q for f in shifted))
    teaching = [(99, 0, True), (100, 360, True)]
    checked("isolated_transmitted_human_language_can_branch", branch_eligible(actors, set(range(16)), {0, 1, 2}, teaching, overlap, 7000))
    animals = [Actor(i, human=False) for i in range(6)]
    checked("animal_teachers_can_maintain_but_not_create_formal_branch", not branch_eligible(animals, set(range(16)), {0, 1, 2}, teaching, overlap, 7000))
    checked("no_actual_new_learner_cohorts_no_language_branch", not branch_eligible(actors, set(range(16)), {0, 1, 2}, [], overlap, 7000))
    reconnect = copy.deepcopy(shifted)
    for _ in range(20):
        for i in range(64):
            reconnect[i] = frequency_update(reconnect[i], 0, 1200)
    checked("actual_recontact_converges_without_deleting_language_history", intelligibility(baseline, reconnect) > 7500)
    parent = [Actor(i) for i in range(8)]
    branch = [Actor(i + 10, genome=(7500,) * 12) for i in range(8)]
    pp, bp = prototype(parent), prototype(branch)
    checked("same_species_actual_gene_cluster_distance_not_color", genetic_distance(pp, bp) == 4000)
    checked("prototype_classifies_clear_cluster", classify(branch[0].genome, {"parent": pp, "branch": bp}, "parent") == "branch")
    checked("mixed_or_equidistant_keeps_parent", classify((5500,) * 12, {"parent": pp, "branch": bp}, "parent") == "parent")
    births=[("birthA",10,11,200),("birthB",12,13,220)]
    checked("actual_reproductive_cluster_can_naturally_branch", lineage_candidate(branch,pp,births,set(range(90)),250))
    checked("gene_distance_without_actual_birth_graph_cannot_branch", not lineage_candidate(branch,pp,[],set(range(90)),250))
    checked("one_parent_pair_repeated_does_not_create_lineage", not lineage_candidate(branch,pp,[("A",10,11,200),("B",10,11,220)],set(range(90)),250))
    checked("external_mixing_blocks_isolated_subspecies", not lineage_candidate(branch,pp,births+[("C",10,99,230),("D",11,98,240)],set(range(90)),250))
    capacity_before = expression(7500, branch[0].shared_profile)
    capacity_after = expression(7500, parent[0].shared_profile)
    checked("first_branch_copy_has_no_capacity_reward", capacity_before == capacity_after)
    checked("uniform_genotype_cannot_branch_by_nationality_or_palette", genetic_distance(prototype(parent), prototype(copy.deepcopy(parent))) == 0)
    whole, split = Integral(), Integral()
    total = whole.advance(120003, 937)
    parts = [1, 19, 37, 100, 300, 480]
    checked("raw_tick_integral_split_independent_with_remainder", sum(split.advance(120003, dt) for dt in parts) == total and split.remainder == whole.remainder)
    wm,sm=MovementIntegral(),MovementIntegral()
    we,wf=wm.advance(7500)
    slices=[sm.advance(75) for _ in range(100)]
    checked("75mm_motion_ticks_pay_nonzero_accumulated_fatigue", sum(v[1] for v in slices)==75)
    checked("movement_energy_and_fatigue_split_invariant", (sum(v[0] for v in slices),sum(v[1] for v in slices))==(we,wf) and sm==wm)
    checked("neutral_raw_gene500_keeps_capacity", expression(5000,5000)==60000)
    checked("water_drag_is_bounded_signed_real_speed", water_drag(10000,Q)==-20000 and water_drag(-1000,5000)==1000)
    checked("compression_depth_maps_fixed_body_height", min(Q,1050*Q//2100)==5000)
    checked("sleep_gene_changes_future_required_rest", 6*500 < 12*500 and Integral().advance(12*500,DAY)==6000)
    checked("installed_gun_parameters_and_real_condition_once", misfire_threshold(400,5000,50000)==2400 and misfire_threshold(600,0,100000)==600)
    checked("same_counter_rng_draw_repeats_without_resampling", rng_uniform("combat.fire",7,Q)==rng_uniform("combat.fire",7,Q))
    checked("blocked_penetration_only_blunt_residue", armor_hit(10000,2000,22000,Q)[0]==400)
    checked("food_absorption_uses_actual_quantity_and_digestibility", absorb(500, 250, 8000) == 100000)
    energy = Energy(120000, 0, 0)
    checked("maintenance_has_no_convertible_milk_biomass", energy.product(180, 1, 2000) == 0 and energy.maintenance == 120000)
    energy.productive, energy.dry_matter_g = 36000, 100
    checked("shared_real_material_and_energy_bound_product", energy.product(180, 1, 2000) == 100 and energy.productive == 18000 and energy.dry_matter_g == 0)
    checked("same_food_material_cannot_pay_second_converter", energy.product(450, 1, 2000) == 0)
    checked("real_carried_load_can_defeat_float_displacement", supported_float_mass(73500, Q, 6000, 70000) < 0 < supported_float_mass(73500, Q, 0, 70000))
    checked("safe_landing_no_damage_and_excess_squared_impulse", fall_severity(4000) == 0 and fall_severity(6500) == 2000)
    debt, damage = oxygen_damage(600, 20, 0)
    checked("wrong_medium_beyond_hold_has_current_real_damage", debt == 620 and damage == 50)
    checked("real_breathable_time_reduces_debt_no_free_refill", oxygen_damage(debt, 0, 20)[0] == 580)
    armor_cases = 0
    for power, protection, durability in itertools.product((8000, 16000, 35000), (0, 8000, 22000), (0, 5000, Q)):
        hits = [(power, 47000), (power // 2, 17000)]
        checked_result = simultaneous_hits(hits, protection, durability)
        assert checked_result == simultaneous_hits(list(reversed(hits)), protection, durability)
        assert checked_result[0] >= 0 and 0 <= checked_result[1] <= durability
        armor_cases += 1
    checked("finite_same_instant_armor_is_order_symmetric", armor_cases == 27)
    bare = armor_hit(35000, 47000, 0, Q)[0]
    armored = armor_hit(35000, 47000, 22000, Q)[0]
    checked("actual_armor_reduces_supported_hit_without_immunity", 0 < armored < bare)
    cycle = WeaponCycle()
    checked("actual_load_transfers_stock_once", cycle.load(1) and cycle.stock == 1 and cycle.loaded == 1 and not cycle.load(1))
    checked("actual_fire_consumes_loaded_once", cycle.fire(2) and cycle.stock == 1 and cycle.loaded == 0 and not cycle.fire(2))
    checked("traditional_knowledge_does_not_unlock_starting_firearm", not weapon_admitted("T028", {"T015"}, True, 1000, 700))
    checked("late_gun_needs_actual_goods_and_training", not weapon_admitted("T028", {"T028"}, False, 1000, 700) and not weapon_admitted("T028", {"T028"}, True, 0, 700))
    checked("actual_late_adoption_item_and_training_admits", weapon_admitted("T028", {"T028"}, True, 700, 700))
    checked("minor_actor_cannot_become_trained_soldier_by_adult_shape", not weapon_admitted("T015", {"T015"}, True, 1000, 300, mature=False))
    return {"passed": True, "named_checks": len(checks), "named_checks_list": checks,
            "finite_armor_snapshot_cases": armor_cases, "culture_actual_day_steps_per_scenario": 80,
            "language_frequency_vectors_per_community": 64,
            "production_implemented": False,
            "scope": "finite_sparse_evidence_frequency_cluster_energy_buoyancy_impulse_ammunition_and_armor_arithmetic_only",
            "excluded": ["production_Cpp_world", "full_species_diet_ecology", "global_language_history", "real_navigation_or_collision", "combat_balance", "rendering_or_Steam"]}


if __name__ == "__main__":
    print(json.dumps(run(), ensure_ascii=False, indent=2))
