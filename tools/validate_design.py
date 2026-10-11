#!/usr/bin/env python3
"""One batch v0.9 specification/model audit. Never builds a deleted runtime."""
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CHECKS=[]

def check(name,value,detail=None):
    CHECKS.append({'id':name,'passed':bool(value),'detail':detail})
    if not value:raise AssertionError(name+': '+str(detail))

def pairs(values):
    result={}
    for k,v in values:
        if k in result:raise ValueError('Duplicate JSON key '+k)
        result[k]=v
    return result

def read(path):return json.loads((ROOT/path).read_text(),object_pairs_hook=pairs,parse_constant=lambda s:(_ for _ in ()).throw(ValueError(s)))
def sha(path):return hashlib.sha256((ROOT/path).read_bytes()).hexdigest()
def strings(value):
    if isinstance(value,str):yield value
    elif isinstance(value,dict):
        for v in value.values():yield from strings(v)
    elif isinstance(value,list):
        for v in value:yield from strings(v)

def main():
    game=read('data/contracts/game_v0_9.json')
    contracts={p:read(p) for p in game['authoritative_contracts']}
    packs={p:read(p) for p in game['runtime_definition_packs']}
    check('unique_current_contract_ids',len({v['contract_id'] for v in contracts.values()})==len(contracts))
    for path,d in {**contracts,**packs}.items():
        check('v09_'+path,d.get('design_version')=='0.9')
        check('not_production_'+path,d.get('production_implemented') is False)
        for v in strings(d):
            match=re.fullmatch(r'((?:data|docs|tools)/[A-Za-z0-9_./-]+\.(?:json|md|py))(?:#.*)?',v)
            if match:check('reference_'+path+'_'+match.group(1),(ROOT/match.group(1)).is_file())
    source=game['original_source'];check('original_source_bytes',sha(source['path'])==source['sha256'])
    base='d0f79d1d7c014c6f42faccc1ce7ecb477da48046'
    lines=subprocess.check_output(['git','ls-tree','-r',base,'assets','third_party','data/catalogs','data/geo'],cwd=ROOT).decode().splitlines()
    for line in lines:
        meta,path=line.split('\t');data=(ROOT/path).read_bytes()
        blob=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        check('source_asset_'+path,blob==meta.split()[2])
    runtime=contracts['data/contracts/runtime_foundation_v1.json']
    check('fixed_exact_eight_heights',game['terrain_policy']['fixed_heights_mm']=={'deep_ocean':-20000,'close_ocean':-8000,'shallow_water':-2000,'sand':1000,'soil':2000,'hill':16000,'mountain':48000,'high_peak':96000})
    check('no_transition_or_freeheight',all(game['terrain_policy'][k] is False for k in ['arbitrary_height','height_noise','shore_or_selection_edge_transition','smooth_brush','copy_ramp']))
    check('cold_weapon_start',not game['military_progression']['starting_firearms_unlocked'] and not game['military_progression']['starting_guns_powder_cannons_granted'])
    check('clock',game['time_contract']['calendar_day_ticks']==12000 and game['time_contract']['motion_sim_second_ticks']==20)
    for p,v in contracts.items():
        time=v.get('time',v.get('time_contract',{}))
        if isinstance(time,dict):
            for k,n in time.items():
                if 'day' in k and 'tick' in k and isinstance(n,int):check('same_clock_'+p+'_'+k,n==12000,n)
    ecology=contracts['data/contracts/surface_ecology.json']
    civic=contracts['data/contracts/civic_control_v1.json']
    interaction=contracts['data/contracts/interaction_runtime_v1.json']
    check('seventy_pages',len(civic['ui']['pages'])==len(interaction['pages'])==70)
    check('core_civic_schemas_exact', {r['id'] for r in civic['commands']}=={r['id'] for r in interaction['commands']})
    check('expanded_fields_exact',all(p['editable_fields']==[f['field'] for f in q['editable_fields']] for p,q in zip(civic['ui']['pages'],interaction['pages'])))
    for pi,p in enumerate(civic['ui']['pages']):
        for fi,f in enumerate(p['editable_field_bindings']):
            check('schema_binding_'+p['id']+'_'+f['field'],f['schema_lookup']=='data/contracts/interaction_runtime_v1.json#/pages/'+str(pi)+'/editable_fields/'+str(fi)+'/schema')
    check('raw_genome_0_1000',interaction['$defs']['Genome']['properties']['traits']['items']['properties']['allele_a']['maximum']==1000)
    check('typed_natural_kind_absent',not {'water_body','sea','lake','mountain','natural_feature'}&set(interaction['$defs']['Ref']['properties']['kind']['enum']))
    canonical_kinds=runtime['object_ref_wire']['canonical_kind_registry']
    check('same_strict_identity_registry',set(interaction['$defs']['Ref']['properties']['kind']['enum'])==set(canonical_kinds))
    check('registered_record_fields_and_owners',all(v.get('owner') and v.get('required_fields') for v in canonical_kinds.values()))
    for command,allowed in interaction['command_target_context_kinds'].items():
        check('command_target_context_'+command,bool(allowed) and set(allowed)<=set(canonical_kinds))
    tools=contracts['data/contracts/tool_registry_v1.json']
    check('eight_tool_sections',len(tools['sections'])==8)
    check('100_tools_no_smooth',len(tools['tools'])==tools['tool_count']==100 and 'smooth' not in {t['id'] for t in tools['tools']})
    for t in tools['tools']:check('localized_tool_'+t['id'],set(t['label'])=={'zh-CN','en','de'})
    life=packs['data/content/life_profiles_v1.json'];combat=packs['data/content/combat_profiles_v1.json'];economy=packs['data/content/economy_content_v1.json'];tech=packs['data/content/technology_runtime_v1.json']
    species=life['species_profiles'];traits=life['trait_consumers']
    check('19_species',len(species)==19 and len({s['species_id'] for s in species})==19)
    check('61_trait_consumers',len(traits)==61 and len({t['trait_id'] for t in traits})==61)
    check('all_trait_consumed',all(t.get('consumer_owner') and t.get('formula') for t in traits))
    nodes=tech['nodes'];ids={n['id'] for n in nodes};raw=read('data/catalogs/technology.json');rawids={n['id'] for k in ['capabilities','families','slots'] for n in raw[k]}
    removed=set(game['source_catalog_policy']['inactive_source_ids'])
    check('exact408_active',len(nodes)==len(ids)==408 and ids==rawids-removed and len(removed)==16)
    visited=set();active=set()
    def dfs(name):
        if name in active:raise AssertionError('Tech cycle '+name)
        if name in visited:return
        active.add(name)
        for pre in byid[name]['prerequisite_ids']:
            check('tech_dependency_'+name+'_'+pre,pre in ids)
            dfs(pre)
        active.remove(name);visited.add(name)
    byid={n['id']:n for n in nodes}
    for n in nodes:
        dfs(n['id']);check('localized_tech_'+n['id'],set(n['labels'])=={'zh-CN','en','de'} and all(n['labels'].values()))
        check('tech_effect_'+n['id'],bool(n['effects']) and all(e.get('opcode') and e.get('target') for e in n['effects']))
        check('tech_research_'+n['id'],n['research']['contact_work_ticks']>0 and bool(n['research']['experiment_id']))
    targets={t['id']:t for t in economy['operational_targets']}
    experiments=economy['experiment_rules']
    check('408_specific_experiments',len(experiments)==408 and {r['node_id'] for r in experiments.values()}==ids)
    for n in nodes:
        rule=experiments[n['research']['experiment_rule_ref']]
        check('experiment_identity_'+n['id'],rule['node_id']==n['id'] and rule['id']==n['research']['experiment_id'])
        check('experiment_real_measurements_'+n['id'],bool(rule['measurement_predicates']) and bool(rule['required_actual_receipt_classes']) and rule['minimum_distinct_samples']>=3)
        profile=rule.get('profile',economy.get('experiment_common_profile',{}))
        check('experiment_not_completion_checkbox_'+n['id'],profile['requires_actual_facility_tools_qualified_actors_and_nonoverlap'] is True and 'residue_custody' in profile['save'])
        for effect in n['effects']:
            target=targets[effect['target']]
            if effect['opcode']=='ENABLE_FAMILY_OPERATION':
                check('family_real_operation_'+n['id'],bool(target['parameters']) and bool(target['consumer']) and bool(effect.get('enable_recipe')))
            else:
                check('effect_target_parameter_'+n['id'],effect['parameter'] in target['parameters'])
            if n['kind']=='slot':
                parameter=target['parameters'][effect['parameter']]
                check('slot_unique_consumer_'+n['id'],parameter['slot_owner']==n['id'] and bool(parameter['consumer_binding']) and bool(parameter['interpreter_operation']))
    goods={g['id']:g for g in economy['goods']};recipes=economy['recipes'];recipe_ids={r['id'] for r in recipes}
    for g in goods.values():check('localized_good_'+g['id'],set(g['labels'])=={'zh-CN','en','de'})
    for r in recipes:
        for v in r['inputs']+r['outputs']:check('recipe_good_'+r['id']+'_'+v['good'],v['good'] in goods and v['quantity']>=0)
        input_mass=sum(v['quantity']*goods[v['good']]['mass_g_per_quantity_unit'] for v in r['inputs'])+(r['source_debit'] or {}).get('mass_g',0)
        output_mass=sum(v['quantity']*goods[v['good']]['mass_g_per_quantity_unit'] for v in r['outputs'])+r['loss_mass_g']+r.get('embedded_living_input_mass_g',0)
        check('recipe_mass_'+r['id'],input_mass==output_mass,{'input':input_mass,'output_loss':output_mass})
    for w in combat['weapon_profiles']:
        check('weapon_good_'+w['id'],w['commodity_id'] in goods)
        for k in ['technology_capability_id','product_family_id']:
            if w[k] is not None:check('weapon_tech_'+w['id']+'_'+k,w[k] in ids)
        if w['ammo_commodity_id']:check('weapon_ammo_'+w['id'],w['ammo_commodity_id'] in goods)
        if w['stage'].startswith('late_'):check('late_weapon_gated_'+w['id'],bool(w['technology_capability_id']) and bool(w['product_family_id']))
    check('UI_absolute_int64_decimal_wire',interaction['$defs']['Order']['properties']['expires_tick']['$ref']=='#/$defs/Int64Wire')
    for command in interaction['commands']:
        if command['id'] in ['IssueMetaOrder','PossessActor','RenewControlLease','ReleaseControlLease','ReleasePossession']:
            check('lease_revision_wire_'+command['id'],command['payload_schema']['properties']['expected_lease_revision']['$ref']=='#/$defs/PositiveInt64Wire')
    # Link-check only current specifications; historical source research links are snapshots.
    links=0
    current=[ROOT/'README.md',ROOT/'AGENTS.md',ROOT/'docs/design/Sonnheide_Design_v0.9_Executable_Rules.md',*sorted((ROOT/'docs/architecture').glob('*.md')),ROOT/'docs/planning/IMPLEMENTATION_PLAN.md',ROOT/'docs/planning/VALIDATION.md',ROOT/'docs/decisions/0018-fixed-terrain-and-executable-game-specification.md']
    for p in current:
        for dest in re.findall(r'\]\(([^)]+)\)',p.read_text()):
            if dest.startswith(('https:','http:','#')):continue
            target=dest.split('#')[0]
            if not target:continue
            if target.endswith('DESIGN_AUDIT_ADR0018.json'):continue
            check('markdown_'+str(p.relative_to(ROOT))+'_'+target,(p.parent/target).resolve().is_file());links+=1
    # Reuse only byte-identical finite model code with its previous narrow scope.
    prior=read('docs/planning/DESIGN_AUDIT_ADR0017.json');reused=[]
    new_names=['world_runtime','emergence_life_combat','content_bootstrap','interaction_schema']
    new_paths=['docs/research/models/'+n+'_reference_model.py' for n in new_names]
    for path,oldsha in prior['contract_and_reference_model_sha256'].items():
        if path.endswith('_reference_model.py') and (ROOT/path).is_file():
            check('unchanged_historical_model_'+path,sha(path)==oldsha)
            reused.append({'path':path,'sha256':oldsha,'executed_this_batch':False,'result_source':'docs/planning/DESIGN_AUDIT_ADR0017.json','scope':'historical_unchanged_finite_submodel_only'})
    results=[];model_failures=[]
    for path in new_paths:
        completed=subprocess.run([sys.executable,str(ROOT/path)],cwd=ROOT,text=True,capture_output=True)
        if completed.returncode:
            model_failures.append(path+'\n'+completed.stdout+'\n'+completed.stderr)
            continue
        try:result=json.loads(completed.stdout)
        except ValueError:
            model_failures.append(path+' invalid result JSON\n'+completed.stdout)
            continue
        passed=result.get('passed',True) is True
        CHECKS.append({'id':'finite_model_'+path,'passed':passed,'detail':None})
        if not passed:model_failures.append(path+' reported failure')
        results.append({'path':path,'sha256':sha(path),'executed_this_batch':True,'result':result})
    if model_failures:raise AssertionError('\n'.join(model_failures))
    report={'schema_version':1,'design_version':'0.9','algorithm_revision':'ADR0018-2026-10-09','date':'2026-10-09','audited_parent_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),'type':'executable_spec_and_finite_reference_audit_not_game_runtime','passed':True,'check_count':len(CHECKS),'checks':CHECKS,'preserved_source_asset_files':len(lines),'source_asset_hash_baseline_commit':base,'source_sha256':sha(source['path']),'current_markdown_links':links,'coverage':{'authority_contracts':len(contracts),'runtime_definition_packs':len(packs),'species':len(species),'traits_consumed':len(traits),'active_technology_nodes':len(nodes),'goods':len(goods),'recipes':len(recipes),'page_fields':sum(len(p['editable_fields']) for p in interaction['pages']),'civic_commands':len(civic['commands']),'supplementary_commands':len(interaction['supplementary_commands']),'bottom_tools':len(tools['tools'])},'new_models_executed':results,'reused_unchanged_models':reused,'future_production_scenarios_executed':0,'production_implemented':False,'build_or_CTest_run':False,'does_not_prove':['full_Cpp_game','real_3D_collision_and_nav','all_GSHHG_import','real_UI_or_GPU','OS_power_loss_durability','final_balance_or_capacity','Windows_Steam_runtime'],'definition_and_model_sha256':{p:sha(p) for p in [*contracts,*packs,*new_paths,'tools/generate_interaction_spec.py']}}
    (ROOT/'docs/planning/DESIGN_AUDIT_ADR0018.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'passed':True,'checks':len(CHECKS),'coverage':report['coverage'],'new_reference_models':len(results),'unchanged_models_reused':len(reused),'production_scenarios_executed':0},ensure_ascii=False))
if __name__=='__main__':main()
