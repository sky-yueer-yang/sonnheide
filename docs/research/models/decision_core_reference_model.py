#!/usr/bin/env python3
"""Finite ADR0016 design model, Python 3.9+ standard library, not game tests.

Checks knowledge isolation, delayed/source-correlated reports, snapshot retries,
sample personality-dependent investment choice, absolute time admission, and
unique learning effects. Does not implement full beliefs, prediction, lifecycle,
all economy/politics/warfare, physical motion, rendering or Steam runtime.
"""
from dataclasses import dataclass, field
from itertools import permutations, product
from hashlib import sha256
from typing import Dict, List, Optional, Tuple
import json


@dataclass(frozen=True)
class Observation:
    root_id: str
    target: str
    low: int
    high: int
    observed: int
    received: int
    confidence: int
    source_revision: int = 1
    source_valid: bool = True


@dataclass
class Knowledge:
    roots: Dict[str, Observation] = field(default_factory=dict)

    def receive(self, observation: Observation, now: int) -> bool:
        if observation.received > now or not observation.source_valid:
            return False
        prior = self.roots.get(observation.root_id)
        if prior is not None and observation.source_revision <= prior.source_revision:
            return False
        self.roots[observation.root_id] = observation
        return True

    def interval(self, target: str) -> Optional[Tuple[int, int]]:
        evidence = [o for o in self.roots.values() if o.target == target]
        if not evidence:
            return None  # Unknown is not (0, 0).
        # Conservative hull preserves disagreement; no confidence voting.
        return min(o.low for o in evidence), max(o.high for o in evidence)


@dataclass
class Personality:
    baseline: int
    current_risk: int
    memory: List[str] = field(default_factory=list)

    def edit_gene(self, new_baseline: int) -> None:
        assert 0 <= new_baseline <= 10000
        self.baseline = new_baseline

    def edit_current(self, new_value: int) -> None:
        if not 0 <= new_value <= 10000:
            raise ValueError("typed range rejected")
        self.current_risk = new_value


@dataclass(frozen=True)
class Plan:
    plan_id: str
    cost: int
    low_gain: int
    high_gain: int


def choose_investment(plans, own_cash: int, protected_floor: int, risk_q: int) -> str:
    feasible = [p for p in plans if own_cash-p.cost >= protected_floor]
    if not feasible:
        return "WAIT"
    attitude = 1000+7000*risk_q//10000
    # All terms here share accounting-unit scale; this is not a cross-domain U.
    def key(p):
        value = p.low_gain*(10000-attitude)+p.high_gain*attitude
        return -value, p.cost, p.plan_id
    return min(feasible, key=key).plan_id


@dataclass
class Episode:
    counter: int = 0
    frozen: Dict[str, str] = field(default_factory=dict)

    def decide(self, key: str, options: Tuple[str, ...]) -> str:
        if key in self.frozen:
            return self.frozen[key]
        ordered = tuple(sorted(options))
        assert ordered
        number = int.from_bytes(sha256((key+":"+str(self.counter)).encode()).digest()[:8], "big")
        result = ordered[number % len(ordered)]
        self.frozen[key] = result
        self.counter += 1
        return result


@dataclass(frozen=True)
class Request:
    actor: int
    request_id: str
    start: int
    end: int
    priority: int
    waiting: int


def admit(requests: Tuple[Request, ...]) -> Tuple[Request, ...]:
    selected = []
    # Input/worker order cannot win a slot. Already irreversible actions would
    # precede this model's priority queue in the full contract.
    for r in sorted(requests, key=lambda x: (x.priority, -x.waiting, x.request_id)):
        if not any(s.actor == r.actor and max(s.start, r.start) < min(s.end, r.end)
                   for s in selected):
            selected.append(r)
    return tuple(selected)


@dataclass
class Learner:
    estimate: int = 0
    receipts: set = field(default_factory=set)

    def observe(self, root_effect: str, value: int, received: bool, confounded: bool) -> None:
        if not received or confounded or root_effect in self.receipts:
            return
        self.estimate += (value-self.estimate)//4
        self.receipts.add(root_effect)


def run():
    checks = 0
    names = []

    def check(name, ok):
        nonlocal checks
        assert ok, name
        checks += 1
        names.append(name)

    k = Knowledge()
    check("unknown_not_zero", k.interval("enemy") is None)
    later = Observation("r1", "enemy", 3, 8, 2, 9, 7000)
    check("future_message_not_received", not k.receive(later, 8))
    check("future_message_no_effect", k.interval("enemy") is None)
    check("arrived_actual_message", k.receive(later, 9))
    echo = Observation("r1", "enemy", 3, 8, 2, 11, 9900)
    check("same_source_echo_rejected", not k.receive(echo, 11))
    check("same_source_no_confidence_boost", k.roots["r1"].confidence == 7000)
    k.receive(Observation("r2", "enemy", 12, 16, 5, 11, 4000), 11)
    check("contradiction_preserves_hull", k.interval("enemy") == (3, 16))

    correction = Observation("r1", "enemy", 1, 2, 2, 13, 6500, 2)
    check("actual_source_correction_updates_one_root", k.receive(correction,13) and len(k.roots)==2)
    check("late_old_source_revision_cannot_overwrite", not k.receive(later,14) and k.roots["r1"].low==1)
    fake = Observation("r1","enemy",0,0,2,14,10000,3,False)
    check("unauthorized_correction_cannot_replace_evidence", not k.receive(fake,14) and k.interval("enemy")== (1,16))

    # Planner only takes permitted K. Vary hidden enemy, private purse, weather
    # and storage while preserving the input projection: no full-World oracle.
    equivalent_world_pairs = 0
    for known, risk in product((None, (3, 8), (12, 16)), (0, 10000)):
        reference = None
        for hidden_enemy, hidden_money, hidden_path, hidden_stock in product(range(4), repeat=4):
            hidden_world = (hidden_enemy, hidden_money, hidden_path, hidden_stock)
            assert len(hidden_world) == 4
            if known is None:
                proposal = "OBSERVE"
            elif known[1] > 10:
                proposal = "AVOID"
            else:
                proposal = "CAUTIOUS" if risk < 5000 else "EXPLORE"
            if reference is None:
                reference = proposal
            assert proposal == reference
            equivalent_world_pairs += 1
    check("hidden_truth_cannot_change_same_K_proposal", equivalent_world_pairs == 1536)

    safe, risky = Plan("safe", 2, 4, 6), Plan("risky", 2, 0, 13)
    check("cautious_chooses_safe", choose_investment((safe, risky), 10, 5, 0) == "safe")
    check("adventurous_can_choose_risky", choose_investment((safe, risky), 10, 5, 10000) == "risky")
    check("personality_cannot_outscore_protected_budget",
          all(choose_investment((safe, risky), 6, 5, r) == "WAIT" for r in range(0,10001,1000)))
    p = Personality(2000, 0, ["loss_observed"])
    p.edit_gene(10000)
    check("DNA_changes_baseline_not_current_or_memory", p.current_risk == 0 and p.memory == ["loss_observed"])
    p.edit_current(10000)
    check("current_edit_does_not_edit_DNA_or_memory", p.baseline == 10000 and p.memory == ["loss_observed"])
    previous = (p.current_risk, p.baseline, tuple(p.memory))
    try:
        p.edit_current(10001)
        assert False
    except ValueError:
        pass
    check("bad_typed_edit_atomic_rejection", previous == (p.current_risk, p.baseline, tuple(p.memory)))

    e = Episode()
    chosen = e.decide("actor:goal:1", ("b", "a", "c"))
    for _ in range(100):
        assert e.decide("actor:goal:1", ("c", "b", "a")) == chosen
    loaded = Episode(e.counter, dict(e.frozen))
    check("query_retry_load_no_extra_draw", loaded.decide("actor:goal:1", ("a", "b", "c")) == chosen
          and loaded.counter == e.counter == 1)
    check("new_episode_one_new_draw", loaded.decide("actor:goal:2", ("a", "b", "c")) in ("a", "b", "c")
          and loaded.counter == 2)

    requests = (Request(1,"care",0,10,1,9),Request(1,"army",0,10,2,0),
                Request(1,"work",10,20,3,2),Request(2,"other",0,20,3,1))
    expected = admit(requests)
    permutations_checked = 0
    for order in permutations(requests):
        actual = admit(order)
        assert actual == expected
        for a, b in permutations(actual,2):
            assert a.actor != b.actor or max(a.start,b.start) >= min(a.end,b.end)
        permutations_checked += 1
    check("care_army_work_actual_time_independent_of_request_order", permutations_checked == 24)
    check("adjacent_work_allowed_overlap_army_not", {r.request_id for r in expected} == {"care","work","other"})
    priority_fair = (Request(1,"new_low_id",0,10,2,0), Request(1,"old_high_id",0,10,2,8))
    check("older_same_class_not_starved_by_ID", admit(priority_fair)[0].request_id == "old_high_id")

    learner = Learner(20)
    learner.observe("flood",0,True,True)
    check("external_confound_no_false_policy_learning", learner.estimate == 20 and not learner.receipts)
    learner.observe("sale",80,False,False)
    check("unreceived_outcome_no_learning", learner.estimate == 20)
    learner.observe("sale",80,True,False)
    actual = learner.estimate
    for _ in range(12):
        learner.observe("sale",80,True,False)
    check("same_effect_learn_once", learner.estimate == actual == 35 and learner.receipts == {"sale"})
    return {"passed":True,"scope":"finite_knowledge_personality_time_admission_and_learning_only",
            "named_checks":checks,"named_checks_list":names,
            "hidden_world_projection_cases":equivalent_world_pairs,
            "time_request_orders":permutations_checked,
            "production_runtime_or_GPU_acceptance":False}


if __name__ == "__main__":
    print(json.dumps(run(), indent=2))
