# -*- coding: utf-8 -*-
#!/usr/bin/env python3
"""Reproducible ADR0017 snapshot audit. Python 3.9+ standard library.

Run from a full repository checkout with retained historical Git objects:
  python3 docs/research/models/audit_adr0017_design.py
Writes a new audit, runs the changed settlement and new carried-goods models; reuses five unchanged results.
This is not a production build, runtime test, or a claim of game implementation.
Frozen counts and hashes intentionally require review after future design changes.
"""
import json,re,hashlib,subprocess,math,importlib.util,sys
sys.dont_write_bytecode=True
from pathlib import Path
from fractions import Fraction
root=Path(__file__).resolve().parents[3]
checks=[]
def check(k,ok,detail=None):
 if not ok:raise AssertionError((k,detail))
 checks.append({'id':k,'passed':True,'detail':detail})
def unique_pairs(pairs):
 result={}
 for k,v in pairs:
  if k in result:raise ValueError('duplicate JSON key '+k)
  result[k]=v
 return result
def read(p):return json.loads((root/p).read_text(),object_pairs_hook=unique_pairs,parse_constant=lambda x:(_ for _ in ()).throw(ValueError(x)))
game=read('data/contracts/game_v0_8.json');life=read('data/contracts/life_genetics_v1.json');terrain=read('data/contracts/terrain_access_v1.json');civic=read('data/contracts/civic_control_v1.json');tools=read('data/contracts/tool_registry_v1.json');eco=read('data/contracts/surface_ecology.json');pixel=read('data/contracts/pixel_world.json');matrix=read('docs/research/WORLDBOX_ADOPTION_v0.8.json')
settlement=read('data/contracts/settlement_lifecycle_v1.json');action=read('data/contracts/natural_action_v1.json');god=read('data/contracts/god_control_v1.json');vegetation=read('data/contracts/vegetation_generator_v1.json')
decision=read('data/contracts/decision_core_v1.json');political=read('data/contracts/political_decisions_v1.json');economic=read('data/contracts/economic_decisions_v1.json');warfare=read('data/contracts/warfare_decisions_v1.json');social=read('data/contracts/social_decisions_v1.json')
contracts={x['contract_id']:x for x in [game,life,terrain,civic,tools,eco,pixel,settlement,action,god,vegetation,decision,political,economic,warfare,social]}
check('unique_current_contract_ids',len(contracts)==16 and len(game['authoritative_contracts'])==15)
for p in game['authoritative_contracts']:
 c=read(p);check('not_implemented_'+Path(p).stem,c['planned_only'] and not c['production_implemented'])
source=root/'docs/design/Sonnheide_Complete_Design_v0.6.md';sha=hashlib.sha256(source.read_bytes()).hexdigest();check('original_user_source_exact',sha==game['original_source']['sha256'],sha)
# Confirm actual retained sources / art / third-party bytes against the pre-reset public snapshot.
base='d0f79d1d7c014c6f42faccc1ce7ecb477da48046'
raw=subprocess.check_output(['git','ls-tree','-r',base,'assets','third_party','data/catalogs','data/geo'],cwd=root).decode();source_count=0
for line in raw.splitlines():
 prefix,p=line.split('\t');old_sha=prefix.split()[2];f=root/p
 check('source_exists_'+p,f.is_file())
 blob_sha=hashlib.sha1(b'blob '+str(len(f.read_bytes())).encode()+b'\0'+f.read_bytes()).hexdigest()
 check('source_bytes_'+p,blob_sha==old_sha);source_count+=1
check('complete_248_disposition',len(matrix['dispositions'])==matrix['count']==248)
check('no_missing_or_duplicate_research_id',{r['research_id'] for r in matrix['dispositions']}=={'F%03d'%i for i in range(1,249)})
check('all_dispositions_reviewable',all(r['decision'] in ['retain','adopt','adapt','exclude','evidence_boundary'] and r['reason'] and not r['production_implemented'] for r in matrix['dispositions']))
from collections import Counter
check('matrix_counts_exact',dict(Counter(r['decision'] for r in matrix['dispositions']))==matrix['decision_counts'])
check('excluded_user_scope',all(game['scope'][x] is False for x in ['procedural_world_template_library','steam_workshop','steam_cloud','private_horse_system','mounted_transport_and_cavalry_migrated']))
tech=read('data/catalogs/technology.json');allids={r['id'] for k in ['capabilities','families','slots'] for r in tech[k]};off=set(game['source_catalog_policy']['inactive_source_ids']);check('exact_horse_technology_retirement',len(off)==16 and off<=allids and len(allids-off)==408 and game['source_catalog_policy']['technology_source_active_view_total']==408)
check('trait_61',len(life['genetics']['traits'])==life['genetics']['trait_count']==61)
check('trait_unique',len({t['id'] for t in life['genetics']['traits']})==61)
traits={t['id'] for t in life['genetics']['traits']}
check('trait_body_temperature_metabolism_sleep_pain_present',{'strength','cold_tolerance','fasting_tolerance','basal_efficiency','pain_sensitivity','sleep_requirement','rest_recovery_efficiency'}<=traits)
check('trait_no_species_escape_no_reset',all(not t['can_grant_missing_species_capability'] and not t['can_reset_absolute_state'] for t in life['genetics']['traits']))
check('trait_three_locales',all(set(t['labels'])=={'zh-CN','en','de'} and all(t['labels'].values()) for t in life['genetics']['traits']))
check('species_19',len(life['species_catalogue'])==life['species_count']==19 and sum(s['animal'] for s in life['species_catalogue'])==18)
species={s['id']:s for s in life['species_catalogue']};check('immutable_species',all(not s['definition_editable'] and not s['cross_species_breeding'] and s['all_ages_same_adult_body'] and not s['private_mount_asset'] and not s['high_peak_exemption'] for s in species.values()))
check('human_age_source',species['human']['player_placement_game_years']==18 and species['human']['birth_game_years']==0)
check('rabbit_and_cow_distinction',species['rabbit']['trait_envelope_overrides']['ground_speed']['min']>species['cow']['trait_envelope_overrides']['ground_speed']['max'] and species['cow']['trait_envelope_overrides']['strength']['min']>species['rabbit']['trait_envelope_overrides']['strength']['max'])
for s in species.values():
 check('species_bounds_'+s['id'],all(v['min']<=v['max'] and math.isfinite(v['min']) and math.isfinite(v['max']) for v in s['trait_envelope_overrides'].values()))
check('no_animal_genesis',all(not s['cultural_or_language_genesis_founder'] for s in species.values() if s['animal']))
check('double_energy_save',{'maintenance_energy_nonrecoverable','productive_energy_from_real_food','body_wetness','next_physiology_tick'}<=set(life['physiology']['saved_absolute_fields']))
check('positive_profile_quantum',terrain['sample_profile']['cell_size_mm']==2000 and terrain['sample_profile']['height_quantum_mm']==500 and terrain['sample_profile']['ramp_height_quantum_mm']==125 and pixel['grid']['sample_ramp_corner_height_quantum_mm']==125)
check('coarse_quantum_cannot_be_car_ramp',Fraction(500,2000)>Fraction(1,8))
# Verify the actual corner sequence of a 12m climb, not just a claim about mean grade.
car=[250*i for i in range(49)];walk=[((i*600)//20)*125 for i in range(21)]
# 12m/40m -> 96 ramp quanta over 20 segments, use a 4/5-quanta allocation.
walk=[((i*96)//20)*125 for i in range(21)]
check('actual_96m_car_ramp',car[-1]==12000 and all(h%125==0 for h in car) and all(Fraction(car[i+1]-car[i],2000)<=Fraction(1,8) for i in range(48)))
check('actual_40m_walk_ramp',walk[-1]==12000 and all(h%125==0 for h in walk) and all(Fraction(walk[i+1]-walk[i],2000)<=Fraction(1,3) for i in range(20)))
check('eight_bottom_sections',len(tools['sections'])==8 and len({s['id'] for s in tools['sections']})==8 and civic['ui']['toolbar_section_count']==8)
check('101_unique_tools',len(tools['tools'])==tools['tool_count']==101 and len({t['id'] for t in tools['tools']})==101)
check('tool_three_locales_and_bottom_only',all(set(t['label'])=={'zh-CN','en','de'} and t['global_entry']=='bottom_toolbar' and t['section'] in {s['id'] for s in tools['sections']} for t in tools['tools']))
check('no_button_border_and_menu_icons',not tools['borders_outlines_rings'] and tools['main_menu_pure_text'] and not civic['ui']['main_menu_icons'])
pages=civic['ui']['pages'];pageids={p['id'] for p in pages};check('70_unique_pages',len(pages)==len(pageids)==70)
relation_count=sum(len(p['relations']) for p in pages)
check('all_page_relationships_resolve',all(r['target_page'] in pageids and r['typed_ref'] and r['archive_navigation'] for p in pages for r in p['relations']),relation_count)
check('all_pages_three_locales',all(set(p['label'])=={'zh-CN','en','de'} for p in pages))
rules=civic['world_rules']['rules'];ruleids={r['id'] for r in rules};check('28_unique_world_rules',len(rules)==len(ruleids)==civic['world_rules']['rule_count']==28)
check('all_life_rule_refs_resolve',set(life['world_rules_interface'])<=ruleids)
check('ecology_regrowth_rule_resolves',eco['regrowth']['rule_key']=='wild_vegetation_recovery' and eco['regrowth']['rule_key'] in ruleids)
check('thermal_rule_resolves',terrain['thermal']['world_rule']['id'] in ruleids)
for r in rules:
 check('rule_no_catchup_'+r['id'],not r['catch_up'])
check('tool_page_routes_resolve',all(t['primary_page'] in pageids for t in tools['tools']))
check('tool_command_routes_resolve',all(c in {x['id'] for x in civic['commands']} for t in tools['tools'] for c in t['route']['commands']))
check('tool_routes_not_claimed_connected',all(not t['route']['implemented'] for t in tools['tools']))
check('grant_foundation_version_frozen','founding_charter_version' in civic['rank_and_empire']['grant_binding'] and not civic['rank_and_empire']['compatible_charter_amendments']['grant_fact_mutated'])
check('clock_calendar_360_and_motion_units_explicit',game['time_contract']['days_per_year']==360 and game['time_contract']['motion_vs_calendar_rates_separate_named_conversions'])
check('command_unique',len({c['id'] for c in civic['commands']})==len(civic['commands']))
check('command_no_arbitrary_fact_edit',all(not c['arbitrary_json_overwrite'] and (not c['mutates_world'] or (c['single_writer'] and c['preview_revalidation'])) for c in civic['commands']))
# New contracts: identifiers, algorithm interfaces and authoring completeness.
check('78_central_commands',len(civic['commands'])==civic['command_count']==78)
commandids={c['id'] for c in civic['commands']}
for p in pages:
 check('every_editable_field_bound_'+p['id'],set(p.get('editable_fields',[]))=={b['field'] for b in p.get('editable_field_bindings',[])})
 for b in p.get('editable_field_bindings',[]):
  for key in ['command','preview_command']:
   if key in b:check('edit_route_'+p['id']+'_'+b['field']+'_'+key,b[key] in commandids)
  for key in ['command_alternatives','preview_command_alternatives']:
   if key in b:check('edit_routes_'+p['id']+'_'+b['field']+'_'+key,set(b[key])<=commandids)
check('no_nonexistent_task_or_brain_page_reference','no_new_task_or_Brain_page' in civic['ui']['algorithm_subrecords'] and 'person_or_animal_activity_panels' in civic['ui']['algorithm_subrecords'])
for ident in ['person','animal']:
 p=next(p for p in pages if p['id']==ident)
 check('activity_subrecords_'+ident,p['algorithm_record_contract']=='data/contracts/natural_action_v1.json' and not p['activity_subrecords']['direct_edit_of_motion_or_inventory'])
check('city_is_same_settlement_identity',next(p for p in pages if p['id']=='city')['identity_domain']=='settlement_with_CITY_charter' and settlement['identity']['settlement_city_same_stable_id'])
for name,c in [('settlement',settlement),('action',action),('god',god),('vegetation',vegetation)]:
 check('new_contract_design_only_'+name,c['design_version']=='0.8' and c['planned_only'] and not c['production_implemented'] and c['algorithm_revision']==('ADR0015-2026-10-09' if name=='vegetation' else 'ADR0017-2026-10-09'))
 document=c.get('document')
 if document:check('new_contract_doc_resolves_'+name,(root/document).exists() or (root/'data/contracts'/document).exists())
case_lists=[settlement['acceptance_cases'],action['acceptance_scenarios'],god['acceptance_cases'],vegetation['verification_scenarios']]
case_counts=[len(x) for x in case_lists]
check('193_city_action_god_vegetation_future_cases',case_counts==[50,42,60,41] and sum(case_counts)==193)
for name,cases in zip(['settlement','action','god','vegetation'],case_lists):
 check('unique_future_case_ids_'+name,len({c.get('id',c.get('case_id')) for c in cases})==len(cases))
check('14_god_command_interfaces',len(god['commands'])==god['command_count']==14 and {x['id'] for x in god['commands']}<=commandids)
check('2_generator_command_interfaces',len(vegetation['commands'])==2 and {x['id'] for x in vegetation['commands']}<=commandids)
check('8_god_tools_and_generator_registered',len(god['proposed_global_tools'])==god['proposed_tool_count']==8 and {x['id'] for x in god['proposed_global_tools']}|{vegetation['tool']['id']}<={x['id'] for x in tools['tools']})
check('god_tools_civilisation_section',all(x['section']=='civilisation' for x in god['proposed_global_tools']))
check('eight_actual_geography_types',god['canonical_geography']['classes']==['shallow_water','close_ocean','deep_ocean','sand','soil','hill','mountain','high_peak'])
canonical=god['canonical_geography']
check('geometry_not_color_label',not canonical['geography_label_color_LOD_controls_wipe'] and not canonical['zero_area_boundary_touch_triggers_type_wipe'])
check('same_kind_theme_no_type_wipe',not canonical['theme_only_change_triggers_type_wipe'] and not canonical['same_type_height_or_slope_change_triggers_type_wipe'])
wipe=god['geography_type_wipe']
check('ground_wipe_preserves_live_actor_portable_subtree',wipe['mandatory_after_actual_type_change'] and not wipe['hit_actor_worn_and_carried_goods_deleted'] and not wipe['destroyed_parent_container_live_actor_worn_and_carried_nonlife_descendants_deleted'])
check('life_and_gestation_protected_from_direct_erase','ActorId_and_biological_body' in wipe['live_actor_exception'] and wipe['external_living_gestation'].startswith('protected_GestationRef'))
check('wipe_no_refund_or_resurrection',not wipe['new_item_refund_or_resource_drop_on_god_wipe'] and not wipe['restoring_geometry_restores_stock_tree_building_or_active_city'])
check('wipe_generator_epoch_same_commit',wipe['generator_dose_and_epoch_transition']['same_atomic_write_set'] and not wipe['generator_dose_and_epoch_transition']['renderer_sparse_tree_auto_regeneration'])
check('god_capability_not_payload_consent_or_population',god['authority']['permission_source']=='trusted_dispatcher_PlayerGodCapability_not_payload_isGod' and not god['authority']['god_event_forges_votes_consent_treaty_abdication_or_empire_approval'] and not god['authority']['god_changes_create_actors_or_free_goods'])
check('city_transfer_no_private_owner_or_citizenship_default',not god['city_state_assignment']['default_registered_resident_citizenship_change'] and god['city_state_assignment']['source_state_and_third_party_owner_preserved'])
check('membership_army_actual_transition',god['shared_member_transition_plan']['default_old_domestic_service_policy']=='END_OLD_DOMESTIC_SERVICE' and not god['shared_member_transition_plan']['service_transition_is_faked_voluntary_desertion'])
check('city_accounts_no_goods_ghost_capacity',settlement['identity']['lost_square_capacity']==0 and not settlement['identity']['logic_account_duplicates_physical_goods'])
check('no_title_neighbor_capture_or_city_auto_restore',not settlement['sovereignty_and_restoration']['geographic_change_assigns_title_to_neighbor'] and not settlement['sovereignty_and_restoration']['lost_city_assignment_auto_restores_on_repaint'] and not settlement['sovereignty_and_restoration']['archive_title_can_restore_state'])
check('unborn_not_population_and_archive_id_permanent',not settlement['identity']['unborn_gestation_counts_current_population_or_office_qualification'] and not settlement['identity']['archive_ids_reused'])
check('growth_units_separate_hard_guards_first',not settlement['plan_comparator']['hard_guard_failure_can_be_outscored'] and settlement['plan_comparator']['ecological_and_private_asset_loss_units_separate'] and not settlement['plan_comparator']['unknown_cost_is_zero'])
check('motion_animation_not_authority',not action['authority']['animation_notify_can_mutate_world'] and not action['animation']['root_motion_can_set_World_position'] and not action['authority']['render_camera_lod_can_change_outcome'])
check('motion_no_worker_race_or_partial_effect',not action['authority']['worker_finish_order_changes_behavior_or_path'] and not action['authority']['failed_write_set_partial_commit'] and not action['authority']['duplicate_world_effect_replay'])
check('motion_not_fullworld_unbounded_CBS',not action['bottlenecks']['full_world_optimal_CBS'] and not action['bottlenecks']['bounded_search_completeness_claim'] and action['bottlenecks']['exit_capacity_required_before_grant'])
check('generator_editor_clock_no_wallcatchup',vegetation['input_clock']['domain']=='EditorInputClock_not_SimTick_not_RenderFrame' and not vegetation['input_clock']['late_wall_time_catch_up'])
density=vegetation['density']
check('density_incremental_no_deferred_deficit','new_dose' in density['law'] and 'no_historical_target_deficit_replay' in density['law'] and density['backlog_when_blocked']==0)
check('density_fractional_credit_subunit_and_reset','strictly_less_than_one_tree' in density['fractional_credit'] and {'geography_wipe','explicit_tree_clear','real_completed_felling'}<=set(density['reset_tree_dose_on']))
check('fixed_windows_and_exact_conservative_crown',density['window_grid']['stride_mm']==8000 and density['window_grid']['side_mm']==32000 and not density['window_grid']['tests_all_continuous_sliding_windows'] and not density['crown_octagon']['platform_float_trig_used_for_cap'] and Fraction(141422,100000)**2>2)
check('mixed_habitat_caps_not_each_species_full',len(vegetation['profiles'])==9 and density['mixed_profile_quota']=='sum(count_i/cap_i)<=1_per_actual_habitat_area' and density['counts_existing_all_sources'])
check('vegetation_source_finite_not_warehouse_goods',vegetation['source_modes']['GOD_VEGETATION']['initial_woody_biomass']=='frozen_species_bounded_source_definition' and not vegetation['source_modes']['GOD_VEGETATION']['grants_warehouse_goods'])
check('idle_actor_escape_preserved','EscapeConnection_for_every_affected_mobile_live_Actor_including_idle_without_task_route' in vegetation['protected_dependencies'] and 'original_nav_components_escape_targets_and_portals' in vegetation['transaction']['read_set'])
check('all_contracts_valid_strict_json',all(read(str(p.relative_to(root))) for p in sorted((root/'data/contracts').glob('*.json'))))
# Reuse exact unchanged sources/results; old carry-loss semantics are not reused.
prior=read('docs/planning/DESIGN_AUDIT_ADR0016.json')
check('prior_adr16_results_passed',prior['passed'] and prior['algorithm_revision']=='ADR0016-2026-10-09')
model_results={};reused=[]
for name in ['vegetation_reference_model','decision_core_reference_model','political_decision_reference_model','economic_decision_reference_model','warfare_decision_reference_model']:
 rel='docs/research/models/'+name+'.py';h=hashlib.sha256((root/rel).read_bytes()).hexdigest()
 check('unchanged_reused_model_'+name,h==prior['contract_and_reference_model_sha256'][rel])
 check('prior_finite_model_passed_'+name,prior['finite_model_results'][name]['passed'])
 model_results[name]=prior['finite_model_results'][name]
 reused.append({'model':name,'source_sha256':h,'result_source':'docs/planning/DESIGN_AUDIT_ADR0016.json','executed_this_batch':False,'scope':'unchanged_finite_submodel_only_not_new_geography_carry_proof'})
for name in ['settlement_reference_model','carried_goods_reference_model']:
 p=root/'docs/research/models'/(name+'.py')
 spec=importlib.util.spec_from_file_location(name,p);module=importlib.util.module_from_spec(spec);sys.modules[name]=module;spec.loader.exec_module(module)
 result=module.run();check('finite_model_'+name,result['passed'],result);model_results[name]=result
check('finite_city_original_scope',model_results['settlement_reference_model']['exhaustive_worlds']==4096)
check('finite_vegetation_original_scope',model_results['vegetation_reference_model']['canonical_partitions']==2048 and model_results['vegetation_reference_model']['planting_orders']==720)
# New decision authoring interfaces and causality guards, distinct from runtime tests.
new_contracts={'decision_core':decision,'political':political,'economic':economic,'warfare':warfare,'social':social}
new_cases=[decision['acceptance_cases'],political['verification_scenarios'],economic['acceptance_scenarios'],warfare['verification_scenarios'],social['acceptance_cases']]
check('304_existing_decision_future_scenarios',[len(x) for x in new_cases]==[48,64,72,72,48] and sum(map(len,new_cases))==304)
for (name,c),cases in zip(new_contracts.items(),new_cases):
 check('new_contract_current_planned_'+name,c['design_version']=='0.8' and c['algorithm_revision']==('ADR0017-2026-10-09' if name in ['political','economic','warfare'] else 'ADR0016-2026-10-09') and c['planned_only'] and not c['production_implemented'])
 check('unique_new_case_ids_'+name,len({x.get('id',x.get('case_id')) for x in cases})==len(cases))
 for x in cases:check('future_case_has_expected_result_'+name+'_'+str(x.get('id',x.get('case_id'))),bool(x.get('expected',x.get('expect',x.get('must')))))
net_budget=next(v for v in economic.values() if isinstance(v,dict) and 'cash_formula' in v)
check('net_available_cash_no_double_claim',not net_budget['existing_reservations_subtracted_again_from_available_cash'] and 'not_already_paid_or_reserved' in net_budget['cash_formula'])
# Check path-valued references in all current authoritative contracts.
def contract_paths(v):
 if isinstance(v,dict):
  for x in v.values():yield from contract_paths(x)
 elif isinstance(v,list):
  for x in v:yield from contract_paths(x)
 elif isinstance(v,str) and v.startswith('data/contracts/') and v.endswith('.json'):yield v
for c in contracts.values():
 for path in set(contract_paths(c)):check('authoritative_contract_reference_'+c['contract_id']+'_'+path,(root/path).is_file())
axes=['curiosity','piety','altruism','risk_tolerance','order_preference','ambition']
check('current_personality_unique_life_author',decision['personality']['axes']==axes and decision['personality']['author']=='life.CurrentPersonality' and decision['personality']['export']=='PersonalitySnapshot')
check('q_personality_not_dna_reexpression',decision['personality']['current_scale_q']==[0,10000] and not decision['personality']['gene_edit_resets_current'] and not decision['personality']['current_edit_modifies_genome'])
check('knowledge_planner_truth_projection',not decision['authority']['world_truth_readable_by_planner'] and decision['authority']['world_truth_used_for_actual_commit_guards'] and not decision['authority']['secret_validation_details_returned_to_actor'])
check('one_physical_time_ledger',decision['availability']['record']=='ActorAvailabilityLedger' and not decision['availability']['same_tick_multiple_domain_full_work'] and not decision['availability']['military_replaces_care_with_fake_actor'])
check('no_per_step_ai_approval',not decision['authority']['automatic_decisions_require_player_click'])
check('old_law_survives_signer_new_executor_checked',decision['authority']['effective_law_survives_signer_death'] and decision['authority']['new_signature_and_actual_implementation_require_current_qualified_executor'])
check('highlevel_quota_only_not_due_truncation',decision['performance']['sample_all_highlevel_primitives_per_tick_max']==4096 and decision['performance']['quota_applies_only_to_preemptible_search_not_due_legal_or_physiology_effects'])
check('learning_real_and_unique',not decision['feedback']['unobserved_counterfactual_reward'] and decision['feedback']['receipt_key']==['subjectRef','rootEffect','domain','metric'])
check('9_political_commands_resolve_existing_pages',len(political['command_routes']['new_typed_commands'])==9 and all(c['id'] in commandids and c['target_page'] in pageids and set(c.get('additional_target_pages',[]))<=pageids for c in political['command_routes']['new_typed_commands']))
check('institution_subrecords_command_routes_resolve',all(set(p.get('decision_subrecord_routes',[]))<=commandids for p in pages))
check('personality_edit_uses_known_typed_command','SetCurrentPersonalityAxes' in commandids)
laws=read('data/catalogs/laws.json');law_ids={x['id'] for x in laws['laws']}
coverage=political['source_law_coverage'];entries=coverage['entries']
check('all_35_source_laws_mapped',coverage['entry_count']==len(entries)==len(law_ids)==35 and {e['source_law_id'] for e in entries}==law_ids)
check('all_source_law_branches_specified',all(e['policy_family'] and e['typed_decision_branch'] and e['current_guard_or_clarification'] for e in entries) and not coverage['source_catalog_mutated'])
check('core_hidden_world_finite_scope',model_results['decision_core_reference_model']['hidden_world_projection_cases']==1536 and model_results['decision_core_reference_model']['time_request_orders']==24)
check('military_mobilization_finite_scope',model_results['warfare_decision_reference_model']['exhaustive_mobilization_states']==2048)
# Current portable preservation interface agrees across every consumer.
p=wipe['portable_companion_preservation']
check('portable_recursive_protection_priority',p['recursive_contents'] and p['priority_over_direct_hit_and_destroyed_ancestor'] and not p['extends_upward_to_building_vehicle_boat'])
check('portable_no_owner_or_source_reference_immunity',not p['ownership_reservation_or_future_pickup_grants_protection'] and not p['source_tree_or_source_warehouse_deletion_erases_already_carried_batch'])
check('portable_absolute_state_no_reset',{'Ref','quantity','owner','custodian','durability','wetness','expiry','Actor_relative_attachment'}<=set(p['preserves']) and not p['new_geography_or_load_repairs_restocks_or_refills'])
check('portable_protection_conflict_revalidation','recompute' in p['actual_binding_changed_after_preview'])
check('pixel_same_portable_semantics',not pixel['godterrain']['worn_and_carried_nonliving_in_scope'] and pixel['godterrain']['live_actor_portable_subtree_preserved'])
check('aggregate_same_portable_semantics',not game['authority']['geography_wipe_includes_hit_actor_worn_and_carried_nonliving'])
check('city_same_portable_semantics',not settlement['geography_wipe_interface']['nonlife_carried_worn_goods_in_hit_actor_wiped'] and settlement['geography_wipe_interface']['live_actor_recursive_portable_protection_survives_destroyed_ancestor'])
check('action_grip_only_if_destroyed',action['disaster_and_city_interfaces']['actual_portable_bindings_survive_ground_or_parent_container_loss'] and 'hand_grip' not in action['disaster_and_city_interfaces']['invalidate_same_commit'])
check('economic_actual_carried_shipment_preserved',economic['disaster_interfaces']['destroyed_receiver_does_not_destroy_carried_shipment'])
check('military_chamber_reload_and_bindings_preserved',warfare['terrain_civic']['weapon_binding_and_state_not_reset_by_geography_wipe'])
check('new_carried_scope_finite',model_results['carried_goods_reference_model']['exhaustive_worlds']==4096 and model_results['carried_goods_reference_model']['insertion_orders']==720 and not model_results['carried_goods_reference_model']['production_implemented'])
changed=['game_v0_8','pixel_world','god_control_v1','settlement_lifecycle_v1','natural_action_v1','civic_control_v1','tool_registry_v1','economic_decisions_v1','political_decisions_v1','warfare_decisions_v1']
for name in changed:check('affected_contract_adr17_'+name,read('data/contracts/'+name+'.json')['algorithm_revision']=='ADR0017-2026-10-09')
# Current routing only; immutable original source and historical reports deliberately link to historical paths.
current=[root/'README.md',root/'AGENTS.md',root/'docs/design/Sonnheide_Design_v0.8_Living_Pixel_World.md',*list((root/'docs/architecture').glob('*.md')),*list((root/'docs/planning').glob('*.md')),*list((root/'docs/decisions').glob('*.md')),root/'docs/research/WORLDBOX_ADOPTION_v0.8.md',root/'docs/research/WORLDBOX_ALGORITHMS_AND_SONNHEIDE_2026-10-09.md',root/'docs/research/DECISION_ALGORITHMS_AND_EVIDENCE_2026-10-09.md']
links=0
for p in current:
 for target in re.findall(r'\[[^\]]+\]\(([^)]+)\)',p.read_text()):
  if target.startswith(('http:','https:','#')):continue
  path=target.split('#',1)[0];check('current_link_'+str(p.relative_to(root))+'_'+path,(p.parent/path).exists());links+=1
  if '#D' in target:
   anchor=target.split('#',1)[1];check('design_anchor_'+anchor,('id="'+anchor+'"') in (p.parent/path).read_text())
retire=read('docs/planning/DESIGN_RETIREMENT.json');check('old_files_retired',len(retire['retired_paths'])==40 and all(not (root/p).exists() for p in retire['retired_paths']))
check('no_runtime_or_empty_skeleton_created',all(not (root/p).exists() for p in ['src','apps','client','engine','tools','tests','CMakeLists.txt']))
check('new_stages_not_claimed_complete',game['acceptance']['production_stages_complete']==[] and not game['platform']['current_runtime_exists'])
report={'schema_version':1,'date':'2026-10-09','design_version':'0.8','type':'design_contract_and_finite_model_audit_not_runtime_test','passed':True,'check_count':len(checks),'preserved_source_asset_files':source_count,'current_local_markdown_links':links,'coverage':{'worldbox_research_rows':248,'genetic_traits':61,'species':19,'animal_species':18,'typed_pages':70,'typed_relations':relation_count,'world_rules':28,'global_bottom_tools':101,'civic_commands':len(civic['commands']),'terrain_future_scenarios':len(terrain['acceptance_cases'])},'source_sha256':sha,'source_asset_hash_baseline_commit':base,'matrix_counts':matrix['decision_counts'],'checks':checks,'does_not_prove':['gameplay_implemented','gpu_rendering','physics_runtime','economy_balance','genetics_runtime','save_fault_injection_runtime','Windows_Steam_runtime','capacity_or_frame_rate'],'build_or_CTest_run':False,'reason_no_compile':'no production source or build entry exists after user-authorized reset'}
check('git_diff_whitespace',subprocess.run(['git','diff','--check'],cwd=root,capture_output=True,text=True).returncode==0)
report['algorithm_revision']='ADR0017-2026-10-09'
report['check_count']=len(checks)
report['future_production_acceptance_counts']=dict(zip(['settlement','natural_action','god_control','vegetation'],case_counts))
report['future_production_acceptance_counts'].update(dict(zip(new_contracts,[len(x) for x in new_cases])))
report['future_production_acceptance_total']=497
report['new_future_production_acceptance_total']=4
report['future_production_scenarios_executed']=0
report['finite_model_results']=model_results
report['historical_audit']='docs/planning/DESIGN_AUDIT_ADR0016.json'
report['reused_unchanged_models']=reused
report['new_models_executed_this_batch']=['settlement_reference_model','carried_goods_reference_model']
report['audited_parent_commit']='a0bf297154e1a0634e5d442da9a7f2dcff031b29'
report['current_contract_count']=len(contracts)
report['contract_and_reference_model_sha256']={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*sorted((root/'data/contracts').glob('*.json')),*sorted((root/'docs/research/models').glob('*.py'))]}
(root/'docs/planning/DESIGN_AUDIT_ADR0017.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ['checks']},ensure_ascii=False))
