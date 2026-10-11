"""Finite ADR0016 design checks; deliberately not a game or 3D combat engine.

Python 3.9+ standard library. run() is called by the unified design audit.
The exhaustive domain has eleven boolean mobilization facts (2**11 states).
Weapon and intelligence checks use deliberately small abstract examples; no
physical accuracy, economic balance or production capacity is implied.
"""
from dataclasses import asdict, dataclass, field
from copy import deepcopy
from itertools import product
import json


@dataclass
class Recruit:
    actor_id: int = 1
    mature: bool = True
    body_fit: bool = True
    healthy: bool = True
    army_id: int = 0
    available: bool = True
    civilian_work_slots: int = 1
    military_work_slots: int = 0


@dataclass
class MobilizationWorld:
    revision: int = 1
    lawful: bool = True
    authority: bool = True
    ai_recruitment: bool = True
    civilian_reserve: bool = True
    equipment: int = 1
    food: int = 1
    budget: int = 1
    actor: Recruit = field(default_factory=Recruit)
    army_equipment: int = 0
    army_food: int = 0
    wage_escrow: int = 0
    receipts: set = field(default_factory=set)


def mobilize(world, expected_revision, receipt_id):
    """One all-or-nothing abstract recruitment/resource transaction."""
    if receipt_id in world.receipts:
        return False
    actor = world.actor
    guards = (
        world.revision == expected_revision,
        world.lawful, world.authority, world.ai_recruitment,
        world.civilian_reserve, actor.mature, actor.body_fit, actor.healthy,
        actor.army_id == 0, actor.available,
        world.equipment >= 1, world.food >= 1, world.budget >= 1,
    )
    if not all(guards):
        return False
    # Prepare on a private copy; the observed world has not been partially edited.
    draft = deepcopy(world)
    draft.equipment -= 1
    draft.army_equipment += 1
    draft.food -= 1
    draft.army_food += 1
    draft.budget -= 1
    draft.wage_escrow += 1
    draft.actor.army_id = 1
    draft.actor.available = False
    draft.actor.civilian_work_slots = 0
    draft.actor.military_work_slots = 1
    draft.receipts.add(receipt_id)
    draft.revision += 1
    world.__dict__.update(draft.__dict__)
    return True


@dataclass(frozen=True)
class BeliefPlan:
    plan_id: str
    lawful: bool
    estimate_lo_q: int
    estimate_hi_q: int


def choose_from_belief(plans, risk_q):
    """An illustrative risk preference, not the complete shared scoring kernel.

    WorldTruth is deliberately not an argument. Legal guards stay ahead of
    preference. Score units are all bounded dimensionless utility q here.
    """
    if not 0 <= risk_q <= 10000:
        raise ValueError('personality q outside contract')
    legal = [p for p in plans if p.lawful]
    if not legal:
        return 'wait'
    def key(plan):
        mean_q = (plan.estimate_lo_q + plan.estimate_hi_q) // 2
        score = plan.estimate_lo_q * (10000 - risk_q) + mean_q * risk_q
        return (-score, plan.plan_id)
    return sorted(legal, key=key)[0].plan_id


@dataclass
class PoliticalWarWorld:
    revision: int = 1
    ai_new_wars: bool = True
    authority: bool = True
    quorum: bool = True
    budget: int = 2
    war_declared: bool = False
    army_people: int = 0
    equipment: int = 0
    political_receipts: set = field(default_factory=set)


def declare_war(world, receipt_id, expected_revision, kind='AUTONOMOUS'):
    if receipt_id in world.political_receipts:
        return False
    if kind not in ('AUTONOMOUS', 'EXPLICIT_GOD'):
        return False
    if world.revision != expected_revision:
        return False
    if kind == 'AUTONOMOUS' and not (
        world.ai_new_wars and world.authority and world.quorum and world.budget >= 1
    ):
        return False
    # God is a trusted separate dispatcher mode here, never a request boolean.
    draft = deepcopy(world)
    if kind == 'AUTONOMOUS':
        draft.budget -= 1
    draft.war_declared = True
    draft.political_receipts.add(receipt_id)
    draft.revision += 1
    world.__dict__.update(draft.__dict__)
    return True


def permits_action(world, kind, actual_local_attack=False):
    if kind in ('RESCUE', 'RETREAT', 'EXISTING_WAR_ACTION'):
        return True
    if kind == 'IMMEDIATE_SELF_DEFENSE':
        return bool(actual_local_attack)
    if kind in ('NEW_WAR', 'ORGANIZED_UNDECLARED_OFFENSIVE'):
        return world.ai_new_wars
    return False


@dataclass
class Gun:
    stock: int = 1
    loaded: int = 0
    reload_required: int = 3
    reload_work: int = 0
    last_integrated_tick: int = 0
    phase: str = 'RELOADING'
    fired_receipts: set = field(default_factory=set)


def integrate_reload(gun, to_tick):
    if to_tick < gun.last_integrated_tick:
        raise ValueError('time cannot move backwards')
    elapsed = to_tick - gun.last_integrated_tick
    gun.last_integrated_tick = to_tick
    if gun.phase != 'RELOADING':
        return
    gun.reload_work = min(gun.reload_required, gun.reload_work + elapsed)
    if gun.reload_work == gun.reload_required and gun.stock > 0:
        # Loading changes custody: it is not yet a fired/consumed projectile.
        gun.stock -= 1
        gun.loaded += 1
        gun.phase = 'READY'


@dataclass
class CombatWorld:
    revision: int = 1
    health: dict = field(default_factory=lambda: {1: 1, 2: 1})
    guns: dict = field(default_factory=lambda: {1: Gun(), 2: Gun()})
    known_ceasefire: set = field(default_factory=set)
    shots: dict = field(default_factory=dict)
    projectile_hits: set = field(default_factory=set)
    deaths: set = field(default_factory=set)


@dataclass(frozen=True)
class FireIntent:
    actor_id: int
    target_id: int
    shot_id: str
    permission: bool = True


def prepare_fire(world, intents):
    """All fire is tested against one snapshot, preserving simultaneous fire."""
    result = []
    seen_shots = set()
    used_actors = set()
    for intent in sorted(intents, key=lambda i: (i.actor_id, i.shot_id)):
        gun = world.guns.get(intent.actor_id)
        if (
            gun is not None
            and world.health.get(intent.actor_id, 0) > 0
            and world.health.get(intent.target_id, 0) > 0
            and intent.permission
            and intent.actor_id not in world.known_ceasefire
            and gun.phase == 'READY' and gun.loaded > 0
            and intent.shot_id not in world.shots
            and intent.shot_id not in seen_shots
            and intent.actor_id not in used_actors
        ):
            result.append(intent)
            seen_shots.add(intent.shot_id)
            used_actors.add(intent.actor_id)
    return result


def commit_fire(world, intents, expected_revision):
    if world.revision != expected_revision:
        return 0
    prepared = prepare_fire(world, intents)
    if not prepared:
        return 0
    draft = deepcopy(world)
    for intent in prepared:
        gun = draft.guns[intent.actor_id]
        gun.loaded -= 1
        gun.phase = 'RECOVERING'
        gun.fired_receipts.add(intent.shot_id)
        draft.shots[intent.shot_id] = (intent.actor_id, intent.target_id)
    draft.revision += 1
    world.__dict__.update(draft.__dict__)
    return len(prepared)


def resolve_same_instant_hits(world, shot_ids):
    """Small abstract same-time hit batch, not a trajectory/collision solver."""
    new_ids = sorted(set(shot_ids) - world.projectile_hits)
    if not new_ids:
        return 0
    damage = {}
    for shot_id in new_ids:
        if shot_id not in world.shots:
            raise ValueError('cannot invent an unfired projectile')
        target = world.shots[shot_id][1]
        damage[target] = damage.get(target, 0) + 1
    # A projectile is already fired: current shooter death/ceasefire cannot erase it.
    for target, amount in sorted(damage.items()):
        old = world.health.get(target, 0)
        world.health[target] = max(0, old - amount)
        if old > 0 and world.health[target] == 0:
            world.deaths.add(target)
    world.projectile_hits.update(new_ids)
    world.revision += 1
    return len(new_ids)


def ready_world():
    world = CombatWorld()
    for gun in world.guns.values():
        integrate_reload(gun, 3)
    return world


def run():
    checks = []
    def check(name, condition):
        if not condition:
            raise AssertionError(name)
        checks.append(name)

    exhaustive = 0
    mobilized = 0
    for flags in product((False, True), repeat=11):
        lawful, authority, rule, reserve, mature, fit, healthy, available, equipment, food, budget = flags
        world = MobilizationWorld(
            lawful=lawful, authority=authority, ai_recruitment=rule,
            civilian_reserve=reserve, equipment=int(equipment), food=int(food),
            budget=int(budget), actor=Recruit(mature=mature, body_fit=fit,
                                             healthy=healthy, available=available),
        )
        original = deepcopy(world)
        result = mobilize(world, 1, 'recruit-1')
        if result:
            mobilized += 1
            assert all(flags)
            assert world.equipment + world.army_equipment == original.equipment
            assert world.food + world.army_food == original.food
            assert world.budget + world.wage_escrow == original.budget
            assert world.actor.actor_id == original.actor.actor_id
            assert world.actor.civilian_work_slots + world.actor.military_work_slots == 1
        else:
            assert world == original
        exhaustive += 1
    check('2048_guard_combinations_only_one_can_mobilize', exhaustive == 2048 and mobilized == 1)

    world = MobilizationWorld()
    check('first_actual_mobilization', mobilize(world, 1, 'recruit-1'))
    saved = deepcopy(world)
    check('replayed_mobilization_no_resource_or_people_duplicate',
          not mobilize(world, world.revision, 'recruit-1') and world == saved)
    world.equipment = world.food = world.budget = 2
    saved = deepcopy(world)
    check('second_army_guard_no_duplicate_duty',
          not mobilize(world, world.revision, 'recruit-2') and world == saved)
    fresh = MobilizationWorld()
    old = deepcopy(fresh)
    check('stale_revision_recruitment_no_half_write',
          not mobilize(fresh, 0, 'old') and fresh == old)

    plans = (BeliefPlan('scout', True, 4, 6), BeliefPlan('attack', True, -8, 28))
    check('risk_personality_changes_legal_choice',
          choose_from_belief(plans, 0) == 'scout' and choose_from_belief(plans, 10000) == 'attack')
    illegal = (plans[0], BeliefPlan('attack', False, 1000, 1000))
    check('personality_cannot_select_illegal_plan',
          all(choose_from_belief(illegal, q) == 'scout' for q in range(0, 10001, 100)))
    improved = (plans[0], BeliefPlan('attack', True, 8, 12))
    check('same_personality_different_evidence_different_choice',
          choose_from_belief(plans, 0) != choose_from_belief(improved, 0))
    # Hypothetical hidden worlds are not passed into the planning function.
    hidden_variants = ({'enemy_stock': 0}, {'enemy_stock': 900}, {'enemy_dead': True})
    choices = [choose_from_belief(plans, 6000) for _truth in hidden_variants]
    check('same_belief_hidden_truth_variants_do_not_change_choice', len(set(choices)) == 1)
    check('candidate_order_and_camera_unused',
          choose_from_belief(tuple(reversed(plans)), 6000) == choices[0])
    check('no_legal_candidate_safe_wait', choose_from_belief((illegal[1],), 10000) == 'wait')

    political = PoliticalWarWorld(ai_new_wars=False)
    before = deepcopy(political)
    check('new_war_rule_off_no_declaration_half_write',
          not declare_war(political, 'auto', 1) and political == before)
    check('off_rule_blocks_organized_undeclared_offensive',
          not permits_action(political, 'ORGANIZED_UNDECLARED_OFFENSIVE'))
    check('off_rule_preserves_bounded_actual_self_defense_and_old_war',
          permits_action(political, 'IMMEDIATE_SELF_DEFENSE', True)
          and not permits_action(political, 'IMMEDIATE_SELF_DEFENSE', False)
          and permits_action(political, 'EXISTING_WAR_ACTION')
          and permits_action(political, 'RESCUE'))
    political.ai_new_wars = True
    check('declaration_does_not_create_personnel_or_equipment',
          declare_war(political, 'decl', 1) and political.army_people == 0
          and political.equipment == 0 and political.budget == 1)
    before = deepcopy(political)
    check('declaration_receipt_no_duplicate_fee',
          not declare_war(political, 'decl', political.revision) and political == before)
    invalid = PoliticalWarWorld(quorum=False)
    before = deepcopy(invalid)
    check('real_quorum_required_not_faked',
          not declare_war(invalid, 'missing-vote', 1) and invalid == before)

    gun = Gun(stock=2)
    integrate_reload(gun, 1)
    state_at_pause = deepcopy(gun)
    integrate_reload(gun, 1)
    check('same_tick_reload_no_pause_work', gun == state_at_pause)
    restored = Gun(**asdict(gun))
    integrate_reload(gun, 3)
    integrate_reload(restored, 3)
    check('reload_checkpoint_absolute_progress_same_completion', gun == restored and gun.loaded == 1 and gun.stock == 1)
    reload_total = gun.stock + gun.loaded
    combat = ready_world()
    combat.guns[1] = gun
    check('fire_consumes_loaded_not_stock_again',
          commit_fire(combat, [FireIntent(1, 2, 'one')], 1) == 1
          and combat.guns[1].stock == 1 and combat.guns[1].loaded == 0
          and combat.guns[1].stock + combat.guns[1].loaded == reload_total - 1)
    before = deepcopy(combat)
    check('fire_receipt_replay_no_second_projectile',
          commit_fire(combat, [FireIntent(1, 2, 'one')], combat.revision) == 0 and combat == before)
    unloaded = CombatWorld()
    before = deepcopy(unloaded)
    check('zero_loaded_never_creates_projectile',
          commit_fire(unloaded, [FireIntent(1, 2, 'empty')], 1) == 0 and unloaded == before)
    stale = ready_world()
    before = deepcopy(stale)
    check('stale_combat_commit_does_not_spend',
          commit_fire(stale, [FireIntent(1, 2, 'stale')], 0) == 0 and stale == before)

    both = ready_world()
    intents = [FireIntent(1, 2, 'a'), FireIntent(2, 1, 'b')]
    reversed_world = deepcopy(both)
    check('same_snapshot_both_legal_shots', commit_fire(both, intents, 1) == 2)
    check('input_order_not_actor_survival_advantage',
          commit_fire(reversed_world, list(reversed(intents)), 1) == 2 and reversed_world == both)
    resolve_same_instant_hits(both, ['a', 'b'])
    resolve_same_instant_hits(reversed_world, ['b', 'a'])
    check('same_instant_mutual_hits_both_deaths', both == reversed_world and both.deaths == {1, 2})
    before = deepcopy(both)
    check('repeated_hit_receipts_no_second_damage',
          resolve_same_instant_hits(both, ['b', 'a']) == 0 and both == before)

    ceasefire = ready_world()
    commit_fire(ceasefire, [FireIntent(1, 2, 'old-shot')], 1)
    ceasefire.known_ceasefire = {1, 2}
    ceasefire.health[1] = 0
    check('fired_projectile_survives_shooter_death_and_ceasefire',
          resolve_same_instant_hits(ceasefire, ['old-shot']) == 1 and ceasefire.health[2] == 0)
    ceasefire = ready_world()
    ceasefire.known_ceasefire = {1}
    before = deepcopy(ceasefire)
    check('known_ceasefire_blocks_new_fire_not_by_ammo_refund',
          commit_fire(ceasefire, [FireIntent(1, 2, 'forbidden')], 1) == 0 and ceasefire == before)
    return {
        'passed': True,
        'scope': 'finite_abstract_design_only_not_production_game_3D_physics_or_tactical_balance',
        'exhaustive_mobilization_states': exhaustive,
        'named_check_count': len(checks),
        'named_checks': checks,
        'production_implemented': False,
    }


if __name__ == '__main__':
    print(json.dumps(run(), indent=2))
