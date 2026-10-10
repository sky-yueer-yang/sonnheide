#!/usr/bin/env python3
"""ADR0018 finite content interpreter and startup ledger, Python 3.9 stdlib.

Not a production game/cooker, AI, pathfinder, actor physiology, combat, building
geometry or capacity benchmark. Every check executes a named small abstraction;
no C++/Steam scenario is marked passed. Parent runs run() in the unified batch.
"""
import copy
import hashlib
import itertools
import json
from collections import Counter, deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
Q = 10000


def trunc_div(numerator, denominator):
    return (1 if numerator >= 0 else -1) * (abs(numerator) // denominator)


def ceil_div(numerator, denominator):
    return (numerator + denominator - 1) // denominator


def predicate(value, rule):
    op = rule['operator']
    if op == 'EQ':
        return value == rule['threshold']
    if op == 'GE':
        return value >= rule['threshold']
    if op == 'LE':
        return value <= rule['threshold']
    if op in ('BETWEEN', 'IN_DECLARED_RANGE'):
        return rule['lower'] <= value <= rule['upper']
    raise ValueError('unknown predicate: ' + op)


def operate(spec, value, actual=1234, work=4, capacity=1000000,
            remainder=0, condition_q=10000, wet_q=0, event_draw=9999):
    """The declared typed operations consume a finite fixture, not a score."""
    op = spec['interpreter_operation']
    den = spec.get('measurement_denominator', 1)
    if op == 'LAWFUL_FEATURE_GATE':
        return {'eligible': bool(value), 'granted_goods': 0, 'granted_money': 0}
    if op == 'REQUIRED_TRAINING':
        return {'required_work': value * work, 'eligible': actual >= value * work}
    if op == 'CONTACT_WORK':
        return {'required_work': ceil_div(value * actual, den)}
    if op == 'ELAPSED_WORK':
        return {'due_tick': actual + value, 'extra_actor_work': 0}
    if op == 'DAMAGE_RATE':
        damage, rem = divmod(value * actual + remainder, den)
        return {'damage': min(capacity, damage), 'remainder': rem,
                'remaining': max(0, capacity - damage)}
    if op == 'MEASUREMENT_BOUND':
        return {'observation_error_bound': value * work, 'truth_grant': 0}
    if op == 'FINITE_LOSS_Q':
        lost, rem = divmod(actual * value + remainder, Q)
        return {'lost': lost, 'usable': actual - lost, 'remainder': rem}
    if op == 'RECOVER_EXISTING_Q':
        recovered, rem = divmod(actual * value + remainder, Q)
        return {'recovered': recovered, 'remaining': actual - recovered,
                'remainder': rem}
    if op == 'ENERGY_RATE':
        energy, rem = divmod(value * actual + remainder, den)
        return {'required_energy': energy, 'remainder': rem,
                'allowed_quantity': actual if energy <= capacity else 0}
    if op == 'THROUGHPUT':
        return {'handled': min(value * work, actual, capacity)}
    if op == 'SPACE_PER_UNIT':
        return {'actual_slot_limit': capacity // max(1, value)}
    if op == 'CAPACITY_LIMIT':
        limit = min(value, capacity)
        return {'actual_limit': limit, 'eligible': actual <= limit}
    if op == 'MAXIMUM_CONDITION':
        return {'eligible': actual <= value}
    if op == 'ENERGY_Q_FACTOR':
        energy, rem = divmod(actual*value+remainder,Q)
        return {'required_energy':energy,'remainder':rem}
    if op == 'MINIMUM_INTERFACE':
        return {'eligible': actual >= value}
    if op == 'MISFIRE_THRESHOLD':
        threshold = min(6000, max(0, value + (Q-condition_q)//5 + min(2000, wet_q//5)))
        return {'threshold_q': threshold, 'misfire': event_draw < threshold}
    raise ValueError('unimplemented finite opcode: ' + op)


def experiment_accept(node, rule, observations, paid_work, elapsed,
                      input_debits, sample_ids, goods):
    # Receipt identity and exact quantity are actual project evidence. An old
    # successful observation without the paid profile never gives knowledge.
    if paid_work < node['research']['contact_work_ticks']:
        return False
    if elapsed < node['research']['min_elapsed_ticks']:
        return False
    if len(set(sample_ids)) < rule['minimum_distinct_samples']:
        return False
    need = {i['good']: i['quantity'] for i in node['research']['consumed_inputs']}
    if input_debits != need or any(g not in goods for g in need):
        return False
    return all(p['metric'] in observations and predicate(observations[p['metric']], p)
               for p in rule['measurement_predicates'])


def proportional(total, weights, cursor=0):
    """Integer largest remainder with persistent rotating ties, never ID bias."""
    if not weights or sum(weights.values()) == 0:
        return {key: 0 for key in weights}, total, cursor
    ids = list(weights)
    weight_total = sum(weights.values())
    result = {key: total * weights[key] // weight_total for key in ids}
    remaining = total - sum(result.values())
    positions = {key: (i - cursor) % len(ids) for i, key in enumerate(ids)}
    ranked = sorted(ids, key=lambda key: (-(total*weights[key] % weight_total), positions[key]))
    for key in ranked[:remaining]:
        result[key] += 1
    return result, 0, (cursor + remaining) % len(ids)


class CurrencyScope:
    def __init__(self, definition):
        self.definition = definition
        self.cash = {}
        self.receipt = None
        self.right_settled_minor = {}
        self.cursor = 0

    def initialize(self, event, residents, unpaid_work, paid_right_ids=(), fail=False):
        if self.receipt is not None:
            return self.receipt if self.receipt['event'] == event else 'ALREADY_INITIALIZED'
        # No publish before every prepared posting and receipt balances.
        d = self.definition
        valid = {key: units*d['labor_conversion_minor_units_per_verified_unpaid_work_unit']
                 for key, units in unpaid_work.items() if key not in set(paid_right_ids)}
        labor_total = min(d['unsettled_labor_entitlement_pool_minor_units'], sum(valid.values()))
        labor, unused, cursor = proportional(labor_total, valid, self.cursor)
        grants, equal_unused, cursor = proportional(
            d['resident_equal_allocation_pool_minor_units'], {r: 1 for r in residents}, cursor)
        public = d['public_base_allocation_minor_units'] + equal_unused + (
            d['unsettled_labor_entitlement_pool_minor_units'] - sum(labor.values()))
        entries = Counter({'public': public})
        entries.update(grants)
        entries.update(labor)
        issue = d['fixed_issue_ceiling_minor_units']
        assert sum(entries.values()) == issue
        entries['issuer_counterpart'] = -issue
        assert sum(entries.values()) == 0
        receipt = {'event': event, 'entries': dict(entries), 'issued': issue,
                   'right_value_minor': valid, 'settled_right_minor': labor,
                   'outstanding_right_minor': {k: valid[k]-labor[k] for k in valid},
                   'public_reserve': public, 'cursor': cursor}
        if fail:
            return 'PREPARE_FAILED'
        self.cash = dict(entries)
        self.right_settled_minor = labor
        self.cursor = cursor
        self.receipt = receipt
        return receipt


class ContentLedger:
    def __init__(self, economy):
        self.goods = {g['id']: g for g in economy['goods']}
        self.recipes = {r['id']: r for r in economy['recipes']}
        self.stock = Counter()
        self.source = Counter()
        self.receipts = {}
        self.loss_g = 0
        self.embedded_g = 0

    def execute(self, rid, receipt, count=1, work=None, source_key=None):
        if receipt in self.receipts:
            return self.receipts[receipt]
        r = self.recipes[rid]
        needed = {i['good']: i['quantity']*count for i in r['inputs']}
        source = r['source_debit']
        source_need = source['quantity']*count if source else 0
        if any(self.stock[g] < qty for g, qty in needed.items()):
            return 'INPUT_MISSING'
        if source and self.source[source_key] < source_need:
            return 'SOURCE_MISSING'
        if work is None or work < r['base_contact_work_ticks']*count:
            return 'WORK_MISSING'
        if any(self.stock[g] < 1 for g in r['tools']):
            return 'TOOL_MISSING'
        before = copy.deepcopy((self.stock, self.source, self.loss_g, self.embedded_g))
        for g, qty in needed.items():
            self.stock[g] -= qty
        if source:
            self.source[source_key] -= source_need
        for i in r['outputs']:
            self.stock[i['good']] += i['quantity']*count
        self.loss_g += r['loss_mass_g']*count
        self.embedded_g += r.get('embedded_living_input_mass_g', 0)*count
        self.receipts[receipt] = {'rid': rid, 'count': count, 'before': before,
                                  'outputs': copy.deepcopy(r['outputs'])}
        return self.receipts[receipt]


class BootstrapScene:
    """Four real calendars, real near-source trips, finite recipes and crops.

    Fixtures omit actual triangles/pathfinding/animation/individual phenotype.
    Routes have declared finite length, and each worker has one nonoverlapping
    calendar. Source claims are serial ledger commits; there are no spawned jobs.
    """
    def __init__(self, economy, human, reachable=True, food_override=None):
        self.e = economy
        self.b = economy['bootstrap']
        self.human = human
        self.day_ticks = self.b['game_day_ticks']
        self.ledger = ContentLedger(economy)
        src = self.b['existing_sources']
        self.ledger.source.update({'stone':src['loose_stone_g'], 'wood':src['fallen_wood_g'],
                                  'fiber':src['fiber_plant_reserve_g'], 'seed':src['wild_grain_seed_g'],
                                  'water':src['potable_water_reserve_g']})
        fruit = src['mature_fruit_reserve_initial_g'] if food_override is None else food_override
        self.fruit_batches = [[0, fruit]]
        self.growth_budget = src['actual_growth_biomass_and_water_source_budget_g']
        self.growth_receipts = set()
        self.reachable = reachable
        self.actors = [{'productive':0,'maintenance':human['placement_maintenance_energy_milli'],
                        'cursor':0,'work':0,'move_energy':0,'contact':0,'food':0,
                        'intervals':[],'ration':0} for _ in range(4)]
        self.completed = set()
        self.plot_ready = {}
        self.plot_water_days = Counter()
        self.front = Counter()
        self.auto_due = {}
        self.harvests = 0
        self.crop_growth_budget_g=self.b['agriculture']['first_cycle_growth_biomass_budget_g']
        self.source_tasks = deque(
            [('gather_fallen_wood','wood')]*13 + [('gather_loose_stone','stone')]*7 +
            [('gather_fiber','fiber')]*7 + [('gather_wild_grain_seed','seed')]*2)
        self.craft_tasks = deque(['knap_stone_hand_tool','make_stone_axe','hand_spin','hand_weave']+
                                 ['sew_underwear']*4+['sew_bra']*2)
        self.build_tasks = deque()
        buildings = {b['id']:b for b in economy['building_functions']}
        for bid, times in [('basic_shelter',1),('farm_plot',4)]:
            for number in range(times):
                for row in buildings[bid]['construction_inputs']:
                    remaining = row['quantity']
                    while remaining:
                        qty = min(12000, remaining)
                        self.build_tasks.append((bid, number, row['good'], qty))
                        remaining -= qty
        self.sow_tasks = deque(range(4))
        self.tool_busy_until = 0
        self.expired_fruit_g = 0
        self.productive_food_absorbed = 0
        self.total_growth_g = 0
        self.failure = None

    def food_source_debit(self, qty, now):
        if not self.reachable:
            return False
        available = sum(n for born,n in self.fruit_batches if born+3*self.day_ticks>now)
        if available < qty:
            return False
        remaining = qty
        for batch in self.fruit_batches:
            if batch[0]+3*self.day_ticks<=now:
                self.expired_fruit_g += batch[1]
                batch[1] = 0
            take = min(batch[1], remaining)
            batch[1] -= take
            remaining -= take
        return remaining == 0

    def eat(self, a, qty):
        if a['ration'] < qty:
            return False
        a['ration'] -= qty
        food = self.ledger.goods['fruit']
        energy = qty*food['energy_milli_per_quantity_unit']*food['digestibleQ']//Q
        self.productive_food_absorbed += energy
        a['productive'] = min(self.human['productive_storage_capacity_energy_milli'], a['productive']+energy)
        return True

    def charge(self, a, ticks, work=0, move_energy=0, survival=False):
        basal = self.human['basal_energy_milli_per_game_day']*ticks//self.day_ticks
        productive_need = self.human['work_energy_milli_per_work_unit']*work
        shared_need = basal + move_energy + (productive_need if survival else 0)
        # Prepare arithmetic locally. Failure must leave both energy ledgers
        # untouched; a failed proposal is never an actual energetic action.
        productive=a['productive'];maintenance=a['maintenance']
        if not survival:
            if productive < productive_need:return False
            productive -= productive_need
        if maintenance+productive < shared_need:return False
        paid=min(maintenance,shared_need)
        maintenance-=paid;productive-=shared_need-paid
        a['maintenance']=maintenance;a['productive']=productive
        return True

    def interval(self, a, label, ticks, work=0, carried_g=0, distance_mm=0, survival=False):
        if a['cursor']+ticks+self.b['sleep_ticks_per_day']>self.day_ticks:
            return False
        if a['work']+work>self.b['max_productive_contact_work_ticks_per_day']:
            return False
        load = self.human['maximum_load_g']
        # Outward empty / inward loaded split; actual recursive carry mass used.
        movement = ceil_div(distance_mm*self.human['movement_energy_milli_per_game_m'],1000)
        movement = movement*(Q+Q*carried_g//load)//Q
        backup=copy.deepcopy(a)
        meal_energy_before=self.productive_food_absorbed
        projected=work*self.human['work_energy_milli_per_work_unit']+movement+ticks*10
        while not survival and a['productive']<projected and a['ration']>0:
            if a['cursor']+ticks+60+self.b['sleep_ticks_per_day']>self.day_ticks:break
            if not self.charge(a,60):break
            start=a['cursor'];a['cursor']+=60
            a['intervals'].append((start,a['cursor'],'actual_meal'))
            self.eat(a,min(500,a['ration']))
        if carried_g>load or not self.charge(a,ticks,work,movement,survival):
            a.clear();a.update(backup);self.productive_food_absorbed=meal_energy_before
            return False
        start = a['cursor']; a['cursor'] += ticks; a['work'] += work
        a['move_energy'] += movement; a['contact'] += work
        a['intervals'].append((start,a['cursor'],label))
        return True

    def route_task(self, a, rid, source_key, receipt):
        r = self.ledger.recipes[rid]
        mass = sum(i['quantity']*self.ledger.goods[i['good']]['mass_g_per_quantity_unit'] for i in r['outputs'])
        backup = copy.deepcopy(a)
        if not self.interval(a,receipt,800+r['base_contact_work_ticks'],r['work_units'],mass//2,60000):
            a.clear();a.update(backup);return False
        result = self.ledger.execute(rid,receipt,work=r['work_units'],source_key=source_key)
        if isinstance(result,str):
            a.clear();a.update(backup);self.failure=result;return False
        return True

    def ration_trip(self, a, day, actor):
        first=day==0
        if not self.reachable:
            self.failure='NO_REACHABLE_MATURE_FOOD';return False
        if not self.interval(a,'outbound_to_fruit',400,0,0,30000,survival=first):return False
        now=day*self.day_ticks+a['cursor']
        if first:
            if not self.food_source_debit(100,now):
                self.failure='NO_REACHABLE_MATURE_FOOD';return False
            if not self.interval(a,'direct_survival_contact',60,60,survival=True):return False
            a['ration']=100;assert self.eat(a,100)
        if not self.food_source_debit(1500,now):
            self.failure='NO_REACHABLE_MATURE_FOOD';return False
        if not self.interval(a,'actual_fruit_harvest',100,100,survival=first):return False
        a['ration']+=1500
        if not self.interval(a,'inbound_with_fruit',400,0,1500,30000,survival=first):return False
        if not self.interval(a,'first_daily_meal',60):return False
        assert self.eat(a,500)
        if first and not self.interval(a,'actual_ground_cache_setup',300,300):return False
        return True

    def finish_day(self, a):
        if a['ration']:
            self.eat(a,a['ration'])
        remain=self.day_ticks-a['cursor']
        if not self.charge(a,remain):
            self.failure='BASAL_ENERGY_DEFICIT';return False
        a['intervals'].append((a['cursor'],self.day_ticks,'rest_sleep_and_leisure'))
        assert a['cursor']+self.b['sleep_ticks_per_day']<=self.day_ticks
        assert all(x[1]<=y[0] for x,y in zip(a['intervals'],a['intervals'][1:]))
        return True

    def run_days(self, days=14):
        buildings={b['id']:b for b in self.e['building_functions']}
        for day in range(days):
            receipt='tree_growth_'+str(day)
            growth=min(6000,self.growth_budget) if self.reachable else 0
            if receipt not in self.growth_receipts:
                self.growth_budget-=growth;self.total_growth_g+=growth
                self.fruit_batches.append([day*self.day_ticks,growth]);self.growth_receipts.add(receipt)
            for a in self.actors:
                a.update({'cursor':0,'work':0,'move_energy':0,'contact':0,'intervals':[],'ration':0})
            for actor,a in enumerate(self.actors):
                if not self.ration_trip(a,day,actor):return False
            # Actual water and care after each plot exists; one real actor may
            # do all four only if that actor's calendar/work/energy allows it.
            for plot,ready in list(self.plot_ready.items()):
                if day*self.day_ticks>=ready and plot not in self.completed:
                    if self.plot_water_days[plot]<7 or self.crop_growth_budget_g<5500:
                        self.failure='CROP_GROWTH_INPUT_MISSING';return False
                    rid='harvest_grain';r=self.ledger.recipes[rid]
                    for a in self.actors:
                        if self.interval(a,'grain_harvest',r['work_units']+400,r['work_units'],2625,30000):
                            key='crop_'+str(plot);self.ledger.source[key]=5500;self.crop_growth_budget_g-=5500
                            assert not isinstance(self.ledger.execute(rid,'harvest_'+key,source_key=key,work=r['work_units']),str)
                            self.harvests+=1;self.completed.add(plot);break
                elif plot not in self.completed:
                    watered=False
                    for a in self.actors:
                        # Each actual crop-day needs real water input, not a
                        # calendar success flag. One 1000g draw, carry and care.
                        if self.route_task(a,'fetch_potable_water','water','water_'+str(day)+'_'+str(plot)):
                            assert self.ledger.stock['water']>=1000
                            if self.interval(a,'crop_care',60,60):
                                self.ledger.stock['water']-=1000
                                self.plot_water_days[plot]+=1;watered=True;break
                    if not watered:self.failure='ACTUAL_CROP_WATER_TIME_MISSING';return False
            for actor,a in enumerate(self.actors):
                if day<3:
                    while self.source_tasks:
                        rid,key=self.source_tasks[0]
                        if not self.route_task(a,rid,key,'source_'+str(day)+'_'+str(actor)+'_'+str(len(self.source_tasks))):break
                        self.source_tasks.popleft()
                else:
                    while self.craft_tasks:
                        rid=self.craft_tasks[0];r=self.ledger.recipes[rid]
                        if r['tools'] and day*self.day_ticks+a['cursor']<self.tool_busy_until:break
                        if not self.interval(a,rid,r['work_units'],r['work_units']):break
                        result=self.ledger.execute(rid,'craft_'+str(len(self.craft_tasks)),work=r['work_units'])
                        if isinstance(result,str):self.failure=result;return False
                        self.craft_tasks.popleft()
                        if r['tools']:self.tool_busy_until=day*self.day_ticks+a['cursor']
                    if self.craft_tasks:continue
                    while self.build_tasks:
                        bid,number,g,qty=self.build_tasks[0]
                        if self.ledger.stock[g]<qty:self.failure='BUILD_MATERIAL_MISSING';return False
                        if not self.interval(a,'front_delivery',400,0,qty//2,30000):break
                        self.ledger.stock[g]-=qty;key=(bid,number,g);self.front[key]+=qty
                        self.build_tasks.popleft()
                        spec=buildings[bid]
                        if all(self.front[(bid,number,i['good'])]>=i['quantity'] for i in spec['construction_inputs']):
                            self.auto_due[(bid,number)]=day*self.day_ticks+a['cursor']+spec['construction_elapsed_ticks_after_all_materials_delivered']
                    if self.build_tasks:continue
                    while self.sow_tasks:
                        plot=self.sow_tasks[0]
                        if day*self.day_ticks+a['cursor']<self.tool_busy_until:break
                        due=self.auto_due.get(('farm_plot',plot))
                        if due is None or day*self.day_ticks+a['cursor']<due:break
                        r=self.ledger.recipes['sow_basic_plot']
                        if not self.interval(a,'plot_setup_and_sow',800,800):break
                        result=self.ledger.execute('sow_basic_plot','sow_'+str(plot),work=500)
                        if isinstance(result,str):self.failure=result;return False
                        self.plot_ready[plot]=day*self.day_ticks+a['cursor']+7*self.day_ticks
                        self.tool_busy_until=day*self.day_ticks+a['cursor'];self.sow_tasks.popleft()
            for a in self.actors:
                if not self.finish_day(a):return False
            # Four real dry ground stockpiles, each <=30000g; aggregate fixtures
            # admit only <=4*30000 and do not pretend to be a built warehouse.
            mass=sum(self.ledger.goods[g]['mass_g_per_quantity_unit']*qty for g,qty in self.ledger.stock.items())
            assert mass<=120000
        return not(self.source_tasks or self.craft_tasks or self.build_tasks or self.sow_tasks) and self.harvests==4


def run():
    e=json.loads((ROOT/'data/content/economy_content_v1.json').read_text())
    tech=json.loads((ROOT/'data/content/technology_runtime_v1.json').read_text())
    source_path=ROOT/'data/catalogs/technology.json'
    source=json.loads(source_path.read_text())
    life=json.loads((ROOT/'data/content/life_profiles_v1.json').read_text())
    human=next(s for s in life['species_profiles'] if s['species_id']=='human')
    goods={g['id']:g for g in e['goods']}
    recipes={r['id']:r for r in e['recipes']}
    targets={t['id']:t for t in e['operational_targets']}
    nodes={n['id']:n for n in tech['nodes']}
    checks=[]
    def check(name, condition):
        assert condition, name
        checks.append(name)
    check('version_clock_life_same_raw_ticks', e['clock_profile']['game_day_ticks']==12000==life['time']['ticks_per_game_day'])
    check('source_hash_immutable',tech['source_catalog_sha256']==hashlib.sha256(source_path.read_bytes()).hexdigest())
    source_nodes={n['id']:n for kind in ('capabilities','families','slots') for n in source[kind]}
    check('exact_408_source_minus_16',set(nodes)==set(source_nodes)-set(tech['removed_ids']) and len(nodes)==408 and len(tech['removed_ids'])==16)
    check('40_92_276',Counter(n['kind'] for n in nodes.values())=={'capability':40,'family':92,'slot':276})
    check('three_actual_locales',all(set(x['labels'])=={'zh-CN','en','de'} and all(x['labels'].values()) for x in list(goods.values())+list(recipes.values())+list(nodes.values())+e['building_functions']+e['industries']))
    indegree={k:len(n['prerequisite_ids']) for k,n in nodes.items()}
    successors={k:[] for k in nodes}
    for key,n in nodes.items():
        original=source_nodes[key].get('prerequisite_ids',[n.get('family_id')])
        check('preserved_source_prerequisites_'+key,set(original)<=set(n['prerequisite_ids']))
        for p in n['prerequisite_ids']:
            check('actual_active_prerequisite_'+key+'_'+p,p in nodes)
            successors[p].append(key)
    frontier=deque(k for k,d in indegree.items() if d==0);topological=[]
    while frontier:
        key=frontier.popleft();topological.append(key)
        for p in successors[key]:
            indegree[p]-=1
            if indegree[p]==0:frontier.append(p)
    check('entire_408_DAG',len(topological)==408)
    parameter_executions=0; experiment_cases=0
    for key,n in nodes.items():
        rule=e['experiment_rules'][n['research']['experiment_rule_ref']]
        check('experiment_identity_'+key,rule['node_id']==key and rule['id']==n['research']['experiment_id'])
        observations={}
        for p in rule['measurement_predicates']:
            observations[p['metric']]=p.get('threshold',p.get('lower'))
        inputs={i['good']:i['quantity'] for i in n['research']['consumed_inputs']}
        samples=list(range(rule['minimum_distinct_samples']))
        check('experiment_paid_profile_'+key,experiment_accept(n,rule,observations,n['research']['contact_work_ticks'],n['research']['min_elapsed_ticks'],inputs,samples,goods))
        check('experiment_no_free_work_'+key,not experiment_accept(n,rule,observations,0,n['research']['min_elapsed_ticks'],inputs,samples,goods))
        check('experiment_no_free_input_'+key,not experiment_accept(n,rule,observations,n['research']['contact_work_ticks'],n['research']['min_elapsed_ticks'],{},samples,goods))
        if rule['measurement_predicates']:
            bad=dict(observations);p=rule['measurement_predicates'][0]
            bad[p['metric']]=(p.get('upper',p.get('threshold',0))+1) if p['operator'] in ('LE','EQ','BETWEEN','IN_DECLARED_RANGE') else p['threshold']-1
            check('experiment_bad_measurement_'+key,not experiment_accept(n,rule,bad,n['research']['contact_work_ticks'],n['research']['min_elapsed_ticks'],inputs,samples,goods))
        experiment_cases+=4
        for effect in n['effects']:
            t=targets[effect['target']]
            if effect['opcode']=='UNLOCK_TYPED_FLAG':
                check('actual_capability_gate_'+key,operate(t['parameters']['enabled'],True)['granted_money']==0)
            elif effect['opcode']=='ENABLE_FAMILY_OPERATION':
                check('family_bound_real_recipe_'+key,effect['enable_recipe'] in recipes)
                for parameter,spec in t['parameters'].items():
                    check('typed_operator_'+key+'_'+parameter,spec['interpreter_operation'] in e['consumer_operations'])
                    result=operate(spec,spec['base'])
                    check('finite_parameter_operation_'+key+'_'+parameter,bool(result))
                    parameter_executions+=1
            elif effect['opcode']=='SELECT_SLOT_PARAMETER_VERSION':
                p=t['parameters'][effect['parameter']]
                check('exact_slot_owner_'+key,p['slot_owner']==key and p['unit']==effect['unit'])
                base=p['base'];variant=effect['variant_value']
                for adoption,condition in itertools.product((0,2500,10000),repeat=2):
                    value=base+trunc_div((variant-base)*adoption*condition,100000000)
                    value=min(p['maximum'],max(p['minimum'],value))
                    result=operate(p,value)
                    check('slot_operation_'+key+'_'+str(adoption)+'_'+str(condition),bool(result) and p['minimum']<=value<=p['maximum'])
                    parameter_executions+=1
                # Actual poor-quality/skill prototype cannot simply name itself
                # the improved variant. Its paired improvement must fail.
                actual_low=base+trunc_div((variant-base)*1000*1000,100000000)
                improved=(actual_low-base)*rule['improvement_orientation']
                threshold=next(q['threshold'] for q in rule['measurement_predicates'] if q['metric']=='paired_improvement')
                check('poor_material_skill_not_nominal_variant_'+key,improved<threshold)
            else:
                raise AssertionError('unknown effect '+key)
    mass_checks=0
    for r in recipes.values():
        input_mass=sum(goods[i['good']]['mass_g_per_quantity_unit']*i['quantity'] for i in r['inputs'])
        output_mass=sum(goods[i['good']]['mass_g_per_quantity_unit']*i['quantity'] for i in r['outputs'])
        source_mass=r['source_debit']['mass_g'] if r['source_debit'] else 0
        check('recipe_mass_'+r['id'],input_mass+source_mass==output_mass+r['loss_mass_g']+r.get('embedded_living_input_mass_g',0) and r['loss_mass_g']>=0)
        check('recipe_real_work_'+r['id'],r['work_units']>0 and r['elapsed_min_ticks']>=r['base_contact_work_ticks'])
        check('recipe_tool_good_'+r['id'],all(g in goods for g in r['tools']))
        check('recipe_active_gate_'+r['id'],all(g in nodes for g in r['knowledge_prerequisites']))
        if not r['source_debit']:
            ein=sum(goods[i['good']]['energy_milli_per_quantity_unit']*goods[i['good']]['digestibleQ']*i['quantity'] for i in r['inputs'])
            eout=sum(goods[i['good']]['energy_milli_per_quantity_unit']*goods[i['good']]['digestibleQ']*i['quantity'] for i in r['outputs'])
            check('no_recipe_food_energy_creation_'+r['id'],eout<=ein)
        mass_checks+=1
    check('13_industry_bindings',len(e['industries'])==13 and all(i.get('recipe_bindings') or i.get('transaction_handler_bindings') or i.get('building_function_bindings') for i in e['industries']))
    check('early_charcoal_ink_no_metallurgy_cycle',not recipes['small_surface_charcoal']['knowledge_prerequisites'] and recipes['make_ink']['knowledge_prerequisites']==['T002'])
    check('early_components_no_factory_cycle',recipes['make_mechanical_components']['facility']=='public_workshop')
    for b in e['building_functions']:
        check('building_embedded_mass_'+b['id'],sum(goods[i['good']]['mass_g_per_quantity_unit']*i['quantity'] for i in b['construction_inputs'])==b['construction_mass_embedded_g'])
        check('no_masonry_worker_animation_gate_'+b['id'],b['onsite_masonry_actor_work_ticks']==0 and b['delivery_contacts_require_actual_existing_carriers'])
    for name,rid,gate in [('matchlock','make_F1401','F1401'),('flintlock','make_F1402','F1402'),('light_cannon','make_F1403','F1403'),('field_cannon','make_F1404','F1404')]:
        check('late_actual_weapon_'+name,gate in recipes[rid]['knowledge_prerequisites'] and not e['bootstrap']['starting_clothing_tools_warehouse_money'])
    check('blackpowder_ammunition_late',set(recipes['make_black_powder']['knowledge_prerequisites'])=={'T028','F1405'})
    empty=ContentLedger(e)
    check('no_free_primitive_stone_tool',empty.execute('knap_stone_hand_tool','empty')=='INPUT_MISSING')
    empty.source['stone']=2000
    first=empty.execute('gather_loose_stone','actual',source_key='stone',work=120)
    check('source_receipt_once',empty.execute('gather_loose_stone','actual',source_key='stone',work=120)==first and empty.stock['stone']==2000)
    check('finite_source_no_second_harvest',empty.execute('gather_loose_stone','new',source_key='stone')=='SOURCE_MISSING')
    scene=BootstrapScene(e,human)
    success=scene.run_days()
    check('four_actor_actual_bootstrap_14days',success)
    check('initial_clothing_actual_material',scene.ledger.stock['underwear']==4 and scene.ledger.stock['bra']==2)
    check('actual_seed_growth_and_harvest',scene.harvests==4 and scene.ledger.stock['grain']==20000 and all(n>=7 for n in scene.plot_water_days.values()))
    check('finite_growth_not_load_refill',scene.total_growth_g<=84000 and len(scene.growth_receipts)==14)
    failed=BootstrapScene(e,human,reachable=False)
    check('no_access_no_free_food_or_teleport',not failed.run_days())
    deficient=copy.deepcopy(e)
    deficient['bootstrap']['existing_sources']['mature_fruit_reserve_initial_g']=0
    deficient['bootstrap']['existing_sources']['actual_growth_biomass_and_water_source_budget_g']=0
    failed=BootstrapScene(deficient,human)
    check('God_juvenile_fruit0_no_hidden_startup_food',not failed.run_days())
    deficient=copy.deepcopy(e);deficient['bootstrap']['existing_sources']['fallen_wood_g']=1000
    failed=BootstrapScene(deficient,human)
    check('insufficient_real_material_explicit_failure',not failed.run_days())
    currency_cases=0
    for people in range(5):
        residents=['r'+str(i) for i in range(people)]
        for values in itertools.product((0,1,20000),repeat=3):
            rights={'w'+str(i):v for i,v in enumerate(values)}
            scope=CurrencyScope(e['monetary_initialization'])
            original=scope.initialize('one',residents,rights)
            snap=copy.deepcopy(scope.cash)
            check('currency_balanced_'+str(currency_cases),sum(scope.cash.values())==0 and original['issued']==1000000)
            check('currency_replay_'+str(currency_cases),scope.initialize('one',residents,rights)==original and scope.cash==snap)
            check('currency_competing_init_'+str(currency_cases),scope.initialize('two',residents+['late'],rights)=='ALREADY_INITIALIZED' and scope.cash==snap)
            check('currency_rights_not_double_paid_'+str(currency_cases),all(0<=original['settled_right_minor'][k]<=v for k,v in original['right_value_minor'].items()))
            currency_cases+=1
    scope=CurrencyScope(e['monetary_initialization'])
    check('currency_failed_prepare_no_partial',scope.initialize('a',['r'],{'w':10},fail=True)=='PREPARE_FAILED' and not scope.cash and scope.receipt is None)
    actual=scope.initialize('a',['r'],{'paid':100,'unpaid':100},paid_right_ids=['paid'])
    check('received_ration_not_reissued_wage',actual['right_value_minor']=={'unpaid':1000})
    wins=Counter();cursor=0
    for _ in range(9):
        allocation,_,cursor=proportional(1,{'a':1,'b':1,'c':1},cursor);wins.update(allocation)
    check('largest_remainder_rotates_fair_ties',set(wins.values())=={3})
    clothing=goods['underwear'];damage=0;rem=0;repairs=0;replacement_count=0;cloth_stock=1000;work_paid=0
    for day in range(720):
        delta,rem=divmod(12000+rem,clothing['wear']['denominator']);damage+=delta
        if damage>=clothing['durability_capacity_points']:
            if repairs<3 and cloth_stock>=clothing['repair']['cloth_g_each']:
                cloth_stock-=clothing['repair']['cloth_g_each'];work_paid+=300
                damage-=clothing['durability_capacity_points']//5;repairs+=1
            else:
                assert cloth_stock>=60
                cloth_stock-=60;work_paid+=180;damage=0;rem=0;repairs=0;replacement_count+=1
    check('finite_repairs_create_repeat_actual_clothing_demand',replacement_count>=2 and work_paid>0 and cloth_stock<1000)
    # Partitioned wear must equal one interval; no reloading fraction reset.
    whole=divmod(123456,100)
    for parts in itertools.product((0,1,17,600),repeat=3):
        ticks=list(parts)+[123456-sum(parts)];d=0;r=0
        for dt in ticks:
            dd,r=divmod(dt+r,100);d+=dd
        check('wear_partition_'+str(parts),(d,r)==whole)
    check('formal_absence_no_job_or_wage_exclusion',e['employment_formal_attire_rule']['absence_still_work_and_pay'] and not e['employment_formal_attire_rule']['fine_or_clockin_exclusion'])
    return {'passed':True,'scope':'finite 408 content/typed arithmetic/paid experiment predicates, mass-energy recipe ledgers, four near-source calendars/14day crop startup, one-issue integer currency replay, finite clothing replacement; not C++ game/AI/geometry/Steam',
            'active_technology_nodes':len(nodes),'recipe_mass_checks':mass_checks,
            'typed_parameter_executions':parameter_executions,'experiment_predicate_cases':experiment_cases,
            'currency_worlds':currency_cases,'named_check_count':len(checks),
            'bootstrap_grain_g':scene.ledger.stock['grain'],'bootstrap_clothing':{'underwear':4,'bra':2},
            'future_production_acceptance_passed':0,'production_implemented':False}


if __name__=='__main__':
    print(json.dumps(run(),ensure_ascii=False,indent=2))
