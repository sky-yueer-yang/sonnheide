#!/usr/bin/env python3
"""Materialize typed UI specification; not a production game handler."""
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

def ref(name): return {'$ref': '#/$defs/' + name}
def integer(lo=0, hi=2147483647): return {'type': 'integer', 'minimum': lo, 'maximum': hi}
def string(n=128): return {'type': 'string', 'maxLength': n}
def enum(*values): return {'type': 'string', 'enum': list(values)}
def array(item, cap=256, minimum=0): return {'type': 'array', 'items': item, 'maxItems': cap, 'minItems': minimum}
def obj(props, required=None):
    return {'type': 'object', 'properties': props, 'required': list(props) if required is None else required, 'additionalProperties': False}
def variants(rows):
    return {'oneOf': [obj(dict({'action': {'const': action}}, **props)) for action, props in rows]}

def main():
    civic = json.loads((ROOT/'data/contracts/civic_control_v1.json').read_text())
    tools = json.loads((ROOT/'data/contracts/tool_registry_v1.json').read_text())
    page_ids = [p['id'] for p in civic['ui']['pages']]
    runtime = json.loads((ROOT/'data/contracts/runtime_foundation_v1.json').read_text())
    kinds = sorted(runtime['object_ref_wire']['canonical_kind_registry'])
    defs = {
      'Ref': obj({'world_id': {'type':'string','pattern':'^[0-9a-f]{32}$'}, 'kind': enum(*kinds), 'stable_id': {'type':'string','pattern':'^[0-9a-f]{16}$'}, 'generation': integer(1,4294967295)}),
      'Name': dict(string(128), minLength=1), 'Note': string(4096),
      'Color': {'type':'string','pattern':'^#[0-9a-fA-F]{6}$'},
      'Position': obj({'x_mm':integer(-100000000,100000000),'y_mm':integer(-100000000,100000000),'z_mm':integer(-200000,200000)}),
      'PolicyPriority': integer(0,10000),
      'Budget': obj({k:integer(0,10000) for k in ['food_bp','shelter_bp','clothing_bp','care_bp','education_bp','investment_bp','reserve_bp']}),
      'Education': obj({'learning_bp':integer(0,10000),'practical_bp':integer(0,10000),'teacher_refs':array(ref('Ref'),64),'language_refs':array(ref('Ref'),16)}),
      'Personality': obj({k:integer(0,10000) for k in ['curiosity','piety','altruism','risk_tolerance','order_preference','ambition']}),
      'Genome': obj({'traits':array(obj({'trait_id':string(64),'allele_a':integer(0,1000),'allele_b':integer(0,1000)}),61,1)}),
      'SubspeciesTraits': obj({'traits':array(obj({'trait_id':string(64),'profile_trait':integer(0,1000)}),61,1)}),
      'PlanningFlags': obj({'categories':array(obj({'category':enum('migration','social_contact','exploration','investment','hostile_engagement','nomination','research','reproduction'),'enabled':{'type':'boolean'}}),8)}),
      'Flag': obj({'theme':ref('Color'),'secondary':ref('Color'),'emblem_id':string(64)}),
      'Preset': enum('deep_ocean','close_ocean','shallow_water','sand','soil','hill','mountain','high_peak'),
      'Cells': array(obj({'x':integer(0,16383),'y':integer(0,16383)}),65536,1),
      'Boundary': obj({'mode':enum('service','sovereignty','claim'),'target':ref('Ref'),'cells':ref('Cells'),'operation':enum('assign','erase'),'allow_cross_existing':{'type':'boolean'}}),
      'ReligionStatus': obj({'binding':enum('none','official','required_for_crown'),'religion':{'oneOf':[ref('Ref'),{'type':'null'}]},'effective_tick':integer()}),
      'Law': obj({'law_id':string(64),'choice_id':string(64),'jurisdiction':ref('Ref'),'effective_tick':integer(),'transition_days':integer(0,3600)}),
      'Charter': obj({'clauses':array(obj({'clause_id':string(64),'choice_id':string(64)}),64,1),'effective_tick':integer()}),
      'ReligionPractice': obj({'practice_id':string(64),'intensity_q':integer(0,10000),'scope':enum('teaching','ritual','charity','internal_governance')}),
      'CulturePractice': obj({'practice_id':string(64),'priority_q':integer(0,10000),'scope':enum('work','learning','ritual','naming','mutual_aid')}),
      'Membership': obj({'members':array(ref('Ref'),4096,1),'target':ref('Ref'),'mode':enum('request_join','request_leave','god_assign'),'preserve_citizenship':{'type':'boolean'}}),
      'Transfer': obj({'source':ref('Ref'),'destination':ref('Ref'),'quantity':integer(1),'carrier':ref('Ref'),'contract':{'oneOf':[ref('Ref'),{'type':'null'}]}}),
      'Payment': obj({'source_account':ref('Ref'),'payee_account':ref('Ref'),'amount_minor':integer(1),'currency_id':string(64),'obligation':ref('Ref')}),
      'Ownership': obj({'asset':ref('Ref'),'new_owner':ref('Ref'),'consent_receipts':array(ref('Ref'),16,1)}),
      'Equipment': variants([('equip',{'actor':ref('Ref'),'slot':string(64)}),('unequip',{'actor':ref('Ref'),'destination':ref('Ref')}),('repair',{'project':ref('Ref'),'material_batches':array(ref('Ref'),16,1)}),('transfer',{'transfer':ref('Transfer')})]),
      'Project': variants([('start',{'definition_id':string(64),'footprint':ref('Cells'),'sponsor':ref('Ref'),'orientation':enum('0','90','180','270')}),('upgrade',{'definition_id':string(64),'sponsor':ref('Ref')}),('demolish',{'safe_relocation_plan':ref('Ref')}),('cancel',{'reason':string(512)}),('schedule',{'priority_q':integer(0,10000),'not_before_tick':integer()}),('reestablish',{'site_cells':ref('Cells'),'sponsor':ref('Ref')})]),
      'Style': obj({'style_id':string(64),'theme':ref('Color')}),
      'Production': obj({'recipe_id':string(64),'target_quantity':integer(0),'priority_q':integer(0,10000),'input_account':ref('Ref'),'output_container':ref('Ref')}),
      'Employment': variants([('offer',{'actor':ref('Ref'),'job_id':string(64),'wage_minor_per_day':integer(),'shift_hours':integer(1,12)}),('accept',{'offer':ref('Ref')}),('end',{'employment':ref('Ref'),'effective_tick':integer()})]),
      'Research': obj({'technology_id':string(64),'budget_minor':integer(),'priority_q':integer(0,10000),'funding_account':ref('Ref'),'lead':ref('Ref')}),
      'Licence': variants([('issue',{'knowledge':ref('Ref'),'recipient':ref('Ref'),'fee_minor':integer(),'expires_tick':integer()}),('amend',{'terms_id':string(64),'consents':array(ref('Ref'),16,1)}),('revoke',{'legal_basis':ref('Ref')})]),
      'Adoption': obj({'technology_id':string(64),'knowledge':ref('Ref'),'installation_project':ref('Ref'),'training_plan':ref('Ref')}),
      'Publication': variants([('read',{'actor':ref('Ref')}),('copy',{'actor':ref('Ref'),'project':ref('Ref')}),('publish',{'licence':ref('Ref'),'distribution_contract':ref('Ref')})]),
      'Harvest': obj({'mode':enum('pick','fell','honey','extract','plant'),'actor':ref('Ref'),'quantity':integer(1),'destination':ref('Ref')}),
      'Route': variants([('route',{'destination':ref('Position'),'certificate':ref('Ref')}),('repair',{'project':ref('Ref')}),('exit_contract',{'contract':ref('Ref'),'legal_basis':ref('Ref')})]),
      'Order': obj({'kind':enum('hold','move','defend','attack','retreat','escort','surrender'),'target_position':ref('Position'),'target_ref':{'oneOf':[ref('Ref'),{'type':'null'}]},'expires_tick':integer(),'issuer':ref('Ref')}),
      'Formation': obj({'kind':enum('line','column','wedge','square','loose','siege'),'spacing_mm':integer(500,20000),'frontage_mm':integer(2000,200000),'facing_mdeg':integer(0,359999)}),
      'Engagement': obj({'policy':enum('hold_fire','self_defense','authorized_targets'),'pursuit_limit_mm':integer(0,500000),'return_supply_floor_q':integer(0,10000)}),
      'Supply': obj({'goods':array(obj({'good_id':string(64),'quantity':integer(1)}),64,1),'funding_account':ref('Ref'),'destination':ref('Ref')}),
      'Treaty': obj({'parties':array(ref('Ref'),16,2),'kind':enum('peace','alliance','trade','access','membership'),'clauses':array(obj({'clause_id':string(64),'choice_id':string(64)}),64,1),'effective_tick':integer(),'consents':array(ref('Ref'),16)}),
      'Plot': obj({'kind':enum('petition','reform','coup','rebellion','diplomatic_pressure'),'participants':array(ref('Ref'),64,1),'goal_id':string(64),'priority_q':integer(0,10000)}),
      'Reservation': variants([('cancel',{'reason':string(512)}),('amend',{'quantity':integer(1),'expires_tick':integer(),'counterparty_consent':ref('Ref')})]),
      'Contract': obj({'counterparties':array(ref('Ref'),16,2),'goods':array(obj({'good_id':string(64),'quantity':integer(1)}),64),'amount_minor':integer(),'delivery_point':ref('Ref'),'due_tick':integer(),'terms_id':string(64)}),
      'Ramp': obj({'path_mm':array(obj({'x_mm':integer(-100000000,100000000),'y_mm':integer(-100000000,100000000)}),64,2),'width_mm':integer(2000,16000),'profile':enum('pedestrian','vehicle','animal'),'start_cell':obj({'x':integer(),'y':integer()}),'end_cell':obj({'x':integer(),'y':integer()}),'source':enum('god','civil_project')}),
      'Succession': obj({'mode':enum('absolute_primogeniture','election','limited_election'),'legal_children':enum('biological','biological_and_adopted'),'adult_only':{'type':'boolean'},'designated_successor':{'oneOf':[ref('Ref'),{'type':'null'}]},'fallback':enum('dynasty_then_subject_election','subject_election')}),
      'AdoptionFamily': obj({'child':ref('Ref'),'legal_parent':ref('Ref'),'consents':array(ref('Ref'),4,1)}),
      'Regency': obj({'regent':ref('Ref'),'scope':array(enum('existing_payments','existing_treaties','basic_supply','defense'),4,1),'expires_tick':integer()}),
      'Vote': obj({'seat':ref('Ref'),'candidate_or_choice':ref('Ref'),'session':ref('Ref')}),
      'Nomination': obj({'candidate':ref('Ref'),'nominator_seat':ref('Ref')}),
      'Abdication': obj({'crown':ref('Ref'),'effective_tick':integer(),'acceptance':ref('Ref')}),
      'Estate': obj({'settlement_plan':ref('Ref'),'beneficiaries':array(ref('Ref'),256),'creditor_claims':array(ref('Ref'),256)}),
      'Training': obj({'actor':ref('Ref'),'skill_id':string(64),'teacher':{'oneOf':[ref('Ref'),{'type':'null'}]},'duration_ticks':integer(1,12000),'location':ref('Ref')}),
      'Storage': obj({'allowed_goods':array(string(64),128),'reserve_capacity_units':integer(),'access_policy':enum('owner','authorized_custodian','contract_receivers')}),
      'FiniteSource': obj({'remaining_quantity':integer(),'reason':string(512),'source_budget_id':string(64)}),
      'SaveRequest': variants([('save',{'slot_id':string(64)}),('load',{'slot_id':string(64)}),('export',{'slot_id':string(64),'destination_uri':string(1024)}),('delete',{'slot_id':string(64),'confirmed_slot_revision':integer()}),('import',{'package_uri':string(1024),'expected_sha256':{'type':'string','pattern':'^[0-9a-f]{64}$'}})]),
      'Petition': variants([('submit',{'founding_state':ref('Ref'),'member_states':array(ref('Ref'),64,1),'charter':ref('Charter'),'religion':ref('Ref')}),('resubmit',{'draft_revision':integer(1)}),('withdraw',{'reason':string(512)})]),
      'NamingPractice': obj({'language':{'oneOf':[ref('Ref'),{'type':'null'}]},'pattern_id':string(64),'scope':enum('personal','family','settlement','document')}),
      'CivicRequest': variants([('join',{'state':ref('Ref'),'city':{'oneOf':[ref('Ref'),{'type':'null'}]}}),('leave',{'state':ref('Ref'),'destination':ref('Position')}),('change_residence',{'residence':ref('Ref'),'legal_basis':ref('Ref')})]),
      'WorldRuleValue': {'oneOf':[{'type':'boolean'},integer(),obj({'limits':array(obj({'target':ref('Ref'),'limit':integer()}),256)})]},
      'WorldRule': obj({'rule_id':string(64),'value':ref('WorldRuleValue')}),
      'Lease': obj({'mode':enum('META_CONTROL','ACTOR_POSSESSION'),'scope':ref('Ref'),'actor_refs':array(ref('Ref'),4096,1),'duration_ticks':integer(1,3600)}),
      'Marker': obj({'title':ref('Name'),'position':ref('Position'),'color':ref('Color'),'note':ref('Note')}),
      'Geography': variants([('paint',{'preset':ref('Preset'),'cells':ref('Cells')}),('raise_one_tier',{'cells':ref('Cells')}),('lower_one_tier',{'cells':ref('Cells')}),('flatten',{'preset':ref('Preset'),'cells':ref('Cells')}),('copy',{'source_cells':ref('Cells'),'destination_cell':obj({'x':integer(),'y':integer()})}),('create_ramp',{'ramp':ref('Ramp')}),('remove_ramp',{'ramp_ref':ref('Ref')})]),
      'Clear': obj({'classes':array(enum('decor','plants','mineral_sources','buildings','roads','ramps','ruins','ordinary_animals','sapient_life'),9,1),'cells':ref('Cells'),'mode':enum('safe_clear','explicit_life_death')}),
      'Foundation': obj({'founder':ref('Ref'),'position':ref('Position'),'supporters':array(ref('Ref'),4096),'mode':enum('normal','god_direct')}),
      'Assignment': obj({'city':ref('Ref'),'state':ref('Ref'),'preserve_personal_citizenship':{'const':True}}),
      'Appointment': obj({'office':ref('Ref'),'actor':ref('Ref'),'mode':enum('normal_authorized','god_direct')}),
      'Uplift': obj({'actor':ref('Ref'),'source':{'const':'player_civilization_light'}}),
      'Residence': obj({'residence':ref('Ref'),'household_members':array(ref('Ref'),64,1),'legal_basis':ref('Ref')}),
      'ServiceObligations': obj({'hours_per_day':integer(0,12),'service':enum('administration','education','defense','care'),'authorization':ref('Ref')}),
      'ShippingPolicy': obj({'accepted_contract_types':array(enum('cargo','passenger','escort'),3),'priority_q':integer(0,10000),'reserve_berths':integer(0,64)}),
      'VegetationDose': obj({'stroke_id':string(64),'samples':array(obj({'x_mm':integer(-100000000,100000000),'y_mm':integer(-100000000,100000000),'input_ms':integer(1,125)}),512,1),'radius_mm':integer(1000,64000),'mode':enum('tree','decor','both')}),
      'ConstAxes': obj({a['id']:enum(*a['values']) for a in civic['constitutional_axes']['axes']}) if isinstance(civic['constitutional_axes'],dict) and isinstance(civic['constitutional_axes'].get('axes'),list) else obj({'axis_choices':array(obj({'axis_id':string(64),'choice_id':string(64)}),8,8)})
    }
    mapping = {}
    def fields(schema,*names):
        for n in names: mapping[n]=schema
    fields('Name','display_name','title');fields('Note','description','observation_note')
    fields('Color','color');fields('Name','emblem');fields('Flag','flag_and_theme')
    fields('Budget','budget_preferences','public_budget_preferences','public_service_budget_preferences')
    fields('Education','education_policy','education_preferences')
    fields('Personality','personality_axes_via_life_contract')
    fields('Genome','bounded_gene_edit_via_life_contract','genome_via_life_contract','individual_genome_via_life_contract')
    fields('SubspeciesTraits','bounded_traits_via_life_contract');fields('PlanningFlags','ai_planner_category_flags')
    fields('Membership','membership_request','membership_policy','god_local_or_target_membership_request','actual_member_treaty_request')
    fields('Residence','residence_request');fields('CivicRequest','legal_civic_requests')
    fields('Boundary','service_boundary_draft','territorial_transition_request');fields('ConstAxes','constitution_draft')
    fields('ReligionStatus','religious_status_draft');fields('Ref','official_religion_request')
    fields('Law','law_draft');fields('Charter','charter_amendment_request','lawful_charter_amendment_request')
    fields('ReligionPractice','root_allowed_doctrine_request','root_bounded_doctrine_request')
    fields('CulturePractice','bounded_practice_policy');fields('NamingPractice','naming_practice_request','bounded_learning_or_naming_practice_request')
    fields('Foundation','found_state_request','god_found_state_here_request')
    fields('Assignment','god_assign_city_to_state_request');fields('Foundation','god_city_independence_request')
    defs['CapitalOrCrown']={'oneOf':[obj({'action':{'const':'capital'},'city':ref('Ref')}),obj({'action':{'const':'crown'},'appointment':ref('Appointment')})]};fields('CapitalOrCrown','god_capital_or_crown_assignment_request')
    fields('Style','unlocked_compatible_visual_style','compatible_unlocked_style_request')
    fields('Project','actual_reestablish_request','actual_anchor_reestablish_request','public_project_request','actual_upgrade_or_demolition_request','uncommitted_project_draft','lawful_cancel_or_schedule_request','actual_capacity_project_request','actual_road_construction_or_clear_request','real_expansion_project_request')
    fields('Production','production_preferences','future_production_preferences','unlocked_real_setup_request')
    fields('Employment','employment_or_project_request');fields('Ownership','legal_ownership_request','legal_asset_contract_request')
    fields('Order','orders');fields('Formation','formation_parameters');fields('Engagement','engagement_policy');fields('Supply','supply_request')
    fields('Equipment','actual_equip_repair_or_transfer_request','actual_equip_unequip_repair_transfer_request')
    fields('Transfer','actual_transfer_request');fields('Payment','actual_payment_request')
    fields('Contract','unexecuted_typed_draft_or_lawful_amendment_request');fields('Adoption','actual_adoption_or_licence_request')
    fields('Publication','actual_copy_publication_or_reading_request');fields('Harvest','actual_plant_pick_or_fell_request','actual_extraction_request','actual_honey_harvest_request')
    fields('FiniteSource','finite_source_edit_request');fields('WorldRule','world_rule_request');fields('WorldRuleValue','validated_rule_value')
    fields('Position','position_inside_authority');fields('Reservation','lawful_reservation_cancel_or_amend_request')
    fields('Route','actual_route_repair_or_transport_request','actual_navigation_or_transport_request','actual_alternative_route_or_contract_exit_request')
    fields('Licence','lawful_issue_amend_or_revoke_request');fields('Research','real_budget_priority_or_goal_request')
    fields('Treaty','actual_war_order_or_peace_proposal','unapproved_typed_draft_or_lawful_amendment_request')
    fields('Uplift','uplift_request_via_life_contract');fields('AdoptionFamily','separate_legal_adoption_request')
    fields('Succession','succession_charter_request','lawful_succession_policy_request')
    fields('ServiceObligations','service_obligations');fields('Estate','lawful_settlement_request');fields('Abdication','lawful_abdication_request')
    fields('Regency','regent_designation_request','lawful_limited_delegation_request');fields('Nomination','lawful_nomination');fields('Vote','seat_authorized_vote')
    fields('Plot','lawful_proposal');fields('PolicyPriority','future_plan_priority');fields('Note','cancel_unexecuted_preparation')
    fields('ShippingPolicy','lawful_shipping_service_policy');fields('Ramp','previewed_god_ramp_request','civil_ramp_project_request')
    fields('Ref','remove_ramp_preview');fields('Training','actual_training_or_learning_request')
    fields('Petition','unapproved_draft','resubmit_current_petition_request','withdraw_unapproved_petition_request','empire_petition_request')
    fields('Storage','actual_transfer_or_storage_policy_request');fields('SaveRequest','local_save_load_export_delete_request')
    # Fields offering several real operations use tagged strict unions, not a guessed default action.
    defs['EmploymentOrProject']={'oneOf':[ref('Employment'),ref('Project')]};mapping['employment_or_project_request']='EmploymentOrProject'
    defs['StorageOrTransfer']={'oneOf':[ref('Storage'),ref('Transfer')]};mapping['actual_transfer_or_storage_policy_request']='StorageOrTransfer'
    defs['OrderOrTreaty']={'oneOf':[ref('Order'),ref('Treaty')]};mapping['actual_war_order_or_peace_proposal']='OrderOrTreaty'
    defs['AdoptionOrLicence']={'oneOf':[ref('Adoption'),ref('Licence')]};mapping['actual_adoption_or_licence_request']='AdoptionOrLicence'
    defs['LearningOrNaming']={'oneOf':[ref('Training'),ref('NamingPractice')]};mapping['bounded_learning_or_naming_practice_request']='LearningOrNaming'
    defs['MarkerPosition']=ref('Position');defs['Members']=array(ref('Ref'),4096);mapping['stable_member_refs']='Members'
    inherited={'same_state_fields':'state','same_building_fields':'building'}
    pages=[]
    for page in civic['ui']['pages']:
        fs=[]
        for field in page['editable_fields']:
            fs.extend(next(p['editable_fields'] for p in civic['ui']['pages'] if p['id']==inherited[field]) if field in inherited else [field])
        bound=[]
        for field in dict.fromkeys(fs):
            if field not in mapping: raise ValueError('Unbound field '+field)
            bound.append({'field':field,'schema':ref(mapping[field]),'write_mode':'local_note' if field=='observation_note' else 'versioned_domain_plan','domain_guards_required':True})
        pages.append({'id':page['id'],'editable_fields':bound,'read_only_unknown_fields':True})
    # Primary target belongs to the shared Envelope; payloads carry only the
    # distinct participants/secondary targets required by their domain.
    base={}
    schemas={}
    def commands(ids,props,required=None):
        for c in ids.split(): schemas[c]=obj(dict(base,**props),required)
    commands('RenameEntity',{'name':ref('Name')})
    commands('ProposeStateFoundation PreviewGodStateFoundation GodFoundStateFromActor',{'foundation':ref('Foundation')})
    commands('CommitInitialCrown GodAppointCrownIncumbent GodAppointCityRepresentative',{'appointment':ref('Appointment')})
    commands('ChangeConstitution',{'axes':ref('ConstAxes')})
    commands('ChangeStateLaw',{'law':ref('Law')})
    commands('ChangeOfficialReligion',{'religion':{'oneOf':[ref('Ref'),{'type':'null'}]},'exit_empire_rank_if_required':{'type':'boolean'}})
    commands('SubmitEmpirePetition',{'petition':ref('Petition')})
    commands('ApproveEmpirePetition',{'approved_charter_hash':string(64),'approved_member_refs':array(ref('Ref'),64,1)})
    commands('ResolveCrownVacancy SettleEstate',{'due_event_receipt':ref('Ref')})
    commands('OpenCrownElection',{'constitution_revision':integer(1),'close_tick':integer()})
    commands('NominateCrownCandidate',{'nomination':ref('Nomination')})
    commands('CastCrownBallot',{'vote':ref('Vote')});commands('CastPolicyBallot',{'session':ref('Ref'),'seat':ref('Ref'),'choice':enum('for','against','abstain')})
    commands('CloseCrownElection ClosePolicySession',{'close_tick':integer(),'session_revision':integer(1)})
    commands('DesignateRegent',{'regency':ref('Regency')})
    commands('EndRegency',{'returning_incumbent':ref('Ref')})
    commands('AbdicateCrown',{'abdication':ref('Abdication')})
    commands('CreateLegalAdoption',{'adoption':ref('AdoptionFamily')})
    commands('ChangeDynastyPolicy',{'policy':ref('Succession')})
    commands('PreviewCityBoundary CommitCityBoundary TransferTerritorialSovereignty ClaimUnassignedTerritory',{'boundary':ref('Boundary')})
    commands('CreatePoliticalPlot',{'plot':ref('Plot')})
    commands('AdvancePoliticalPlot',{'completed_episode_receipt':ref('Ref')})
    commands('CancelPoliticalPlot',{'reason':ref('Note')})
    commands('ProposeDiplomaticIntent',{'treaty_draft':ref('Treaty')})
    commands('CommitWarDeclaration',{'war_goal_id':string(64),'opponent':ref('Ref'),'authorization':ref('Ref'),'mobilization_plan':ref('Ref')})
    commands('CommitPeaceTreaty CommitSignedTreaty',{'treaty':ref('Treaty')})
    commands('ChangeAllianceCharter',{'charter':ref('Charter')})
    commands('RecruitExistingActor',{'actor':ref('Ref'),'army':ref('Ref'),'consent_or_legal_authorization':ref('Ref'),'equipment_batches':array(ref('Ref'),16)})
    commands('IssueArmyOrder',{})
    schemas['IssueArmyOrder']=variants([('legal_role',{'order':ref('Order')}),('player_lease',{'order':ref('Order'),'lease':ref('Ref'),'expected_lease_revision':integer(1)})])
    commands('IssueMetaOrder',{'order':ref('Order'),'lease':ref('Ref'),'expected_lease_revision':integer(1)})
    commands('EnterMetaControl EnterMetaSpectate',{'scope':ref('Ref')})
    commands('AcquireControlLease',{'lease':ref('Lease'),'first_order':{'oneOf':[ref('Order'),{'type':'null'}]}})
    commands('ExitMetaControl',{'leases':array(obj({'lease':ref('Ref'),'expected_lease_revision':integer(1)}),4096)})
    commands('ReleaseControlLease ReleasePossession',{'lease':ref('Ref'),'expected_lease_revision':integer(1)})
    commands('RenewControlLease',{'lease':ref('Ref'),'expected_lease_revision':integer(1),'duration_ticks':integer(1,3600)})
    commands('PossessActor',{'actor':ref('Ref'),'lease':ref('Ref'),'expected_lease_revision':integer(1)})
    commands('SetAIPlanningCategory',{'flags':ref('PlanningFlags')})
    commands('SetWorldRule',{'rule':ref('WorldRule')})
    commands('ReorderWorldRuleUI',{'rule_ids':array(string(64),28,28)})
    commands('SetMarker',{'marker':ref('Marker')})
    commands('ClearSelectedClasses',{'clear':ref('Clear')})
    commands('RequestDefenseWallProject',{'project':ref('Project')})
    commands('ChangeGatePermission',{'policy':enum('public','members','authorized_refs','closed'),'authorized_refs':array(ref('Ref'),256)})
    commands('SaveCommittedWorld LoadIsolatedWorld ExportLocalWorld ImportLocalWorld',{'request':ref('SaveRequest')})
    commands('PreviewGeographyMutation CommitGeographyMutation',{'geography':ref('Geography')})
    commands('PreviewLocalMembershipStroke CommitLocalMembershipStroke PreviewCivicMembershipBatch GodSetCivicMembershipBatch',{'membership':ref('Membership')})
    commands('PreviewCityStateAssignment GodTransferCityToState',{'assignment':ref('Assignment')})
    commands('GodSeparateCityAsState',{'city':ref('Ref'),'founder':ref('Ref'),'name':ref('Name')})
    commands('GodSetStateCapital',{'city':ref('Ref')})
    commands('CommitVegetationDoseBatch',{'dose':ref('VegetationDose')})
    commands('EndVegetationStroke',{'stroke_id':string(64)})
    commands('SetCurrentPersonalityAxes',{'personality':ref('Personality')})
    commands('OpenPolicySession',{'proposal':ref('Ref'),'close_tick':integer(),'institution_revision':integer(1)})
    commands('CommitPolicyImplementationBatch',{'policy':ref('Ref'),'execution_plan':ref('Ref')})
    commands('SubmitPoliticalPetition',{'issue_id':string(64),'requested_change':ref('Law'),'signatories':array(ref('Ref'),4096,1)})
    commands('RespondPoliticalPetition',{'response':enum('accept_review','reject','request_information'),'reason':ref('Note')})
    commands('DeliverDiplomaticMessage',{'message':ref('Ref'),'delivery_receipt':ref('Ref')})
    commands('CommitIllicitPoliticalAct',{'plot':ref('Ref'),'action_id':string(64),'actual_participants':array(ref('Ref'),64,1)})
    for c in civic['commands']:
        if c['id'] not in schemas: raise ValueError('Missing command payload '+c['id'])
    supplementary = [
      ('PlaceHuman', obj({'cells':ref('Cells'),'count':integer(1,256)}), 'player_placement_fixed18_adult_body_no_inventory_no_membership'),
      ('PlaceAnimal', obj({'species_id':string(64),'cells':ref('Cells'),'count':integer(1,256)}), 'registered_fixed_species_bounded_source_no_products'),
      ('IlluminateExistingAnimals', obj({'actor_refs':array(ref('Ref'),4096,1)}), 'same_Actor_uplift_no_new_population_culture_or_language'),
      ('EditBoundedGenomeBatch', obj({'actor_refs':array(ref('Ref'),4096,1),'genome':ref('Genome')}), 'fixed_species_raw_alleles_no_resource_refill'),
      ('MoveExistingActorByDivineHand', obj({'actor':ref('Ref'),'destination':ref('Position')}), 'explicit_God_relocation_actual_body_capacity_no_peak_no_free_items'),
      ('SetEcologyTheme', obj({'cells':ref('Cells'),'theme':enum('snowfield','flower_meadow','maple_plain','cherry_wild','wetland','savana','sonnheide_sacred_plain','emberland')}), 'soil_only_theme_no_type_wipe_no_new_trees_or_fruit'),
      ('PlantSeedOrSapling', obj({'plant_definition_id':string(64),'position':ref('Position'),'source':enum('god_vegetation','civil_material_project'),'material_claims':array(ref('Ref'),16)}), 'habitat_spacing_and_routes_god_finite_biomass_no_fruit_civil_real_seed_labor'),
      ('SetSoilMoisture', obj({'cells':ref('Cells'),'moisture_q':integer(0,10000)}), 'soil_property_only_no_inventory_or_crop_progress_refill'),
      ('SetSimulationSpeed', obj({'speed':{'type':'integer','enum':[0,1,4,16,64]}}), 'wallclock_rate_only_pause_freezes_all_authoritative_time'),
      ('SetEnvironmentAge', obj({'age':enum('light','darkness'),'automatic':{'type':'boolean'},'remaining_ticks':integer(1,12000000)}), 'manual_age_atomically_stops_automatic_no_calendar_technology_change'),
      ('CreateRootReligion', obj({'name':ref('Name'),'prophet':ref('Ref'),'doctrine':ref('Charter')}), 'trusted_player_existing_prophet_root_source_freezes_no_free_goods'),
      ('PreviewObjectDraft', obj({'draft':ref('ObjectDraft')}), 'all_registered_page_fields_only_plan_no_world_write'),
      ('CommitObjectDraft', obj({'draft':ref('ObjectDraft')}), 'same_plan_revalidation_delegates_domain_single_writer'),
    ]
    defs['Envelope']=obj({'command_id':string(64),'world_id':{'type':'string','pattern':'^[0-9a-f]{32}$'},'target':ref('Ref'),'expected_target_revision':integer(),'session_generation':integer(1,4294967295),'draft_generation':integer(1,4294967295),'expected_world_revision':integer(),'idempotency_key':string(128),'preview_token':{'oneOf':[string(128),{'type':'null'}]}})
    defs['ObjectDraft']=obj({'target':ref('Ref'),'page_id':enum(*page_ids),'target_revision':integer(),'fields':array({'oneOf':[obj({'field_id':{'const':f},'value':ref(mapping[f])}) for f in sorted(mapping)]},64,1),'view_selector':{'oneOf':[obj({'skill_id':string(64)}),{'type':'null'}]}},required=['target','page_id','target_revision','fields'])
    errors=[('INVALID_FIELD','字段无效','Invalid field','Ungültiges Feld'),('OUT_OF_RANGE','数值超出范围','Value out of range','Wert außerhalb des Bereichs'),('UNKNOWN_PROPERTY','包含未知字段','Unknown property','Unbekanntes Feld'),('BUDGET_SUM','预算比例总和须为100%','Budget shares must total 100%','Budgetanteile müssen 100% ergeben'),('STALE_PREVIEW','后果已变化，请重新预览','Consequences changed; preview again','Folgen geändert; Vorschau erneut öffnen'),('WRONG_WORLD','对象属于其他世界','Object belongs to another world','Objekt gehört zu einer anderen Welt'),('PERMISSION_DENIED','没有此操作权限','Operation is not permitted','Aktion nicht erlaubt'),('NO_REAL_RESOURCE','实际资源不足','Insufficient real resources','Nicht genügend tatsächliche Ressourcen'),('UNREACHABLE','没有可用通路','No usable route','Kein nutzbarer Weg'),('SPECIES_ENVELOPE','超出物种固定范围','Outside the species envelope','Außerhalb der Artgrenzen'),('SOURCE_REQUIRED','缺少合法来源','A valid source is required','Gültige Herkunft erforderlich'),('TECH_LOCKED','技术尚未解锁','Technology is locked','Technologie nicht freigeschaltet'),('FIREARM_LOCKED','枪械尚未研制并装备','Firearm has not been researched and equipped','Feuerwaffe noch nicht erforscht und ausgerüstet'),('OUTSIDE_AUTHORITY','位置超出世界有效范围','Position outside the active world','Position außerhalb der aktiven Welt'),('NATURAL_ENTITY_UNSUPPORTED','自然地理没有对象身份','Natural geography has no entity identity','Natürliche Geografie hat keine Objektidentität'),('FREE_HEIGHT_UNSUPPORTED','请选择固定地形档','Choose a fixed terrain tier','Feste Geländestufe wählen'),('SAVE_FAILED','保存失败，原存档保留','Save failed; previous save retained','Speichern fehlgeschlagen; vorheriger Stand bleibt'),('DEFINITION_MISMATCH','存档所需定义包不匹配','Required definition pack differs','Benötigtes Definitionspaket stimmt nicht überein'),('DEAD_TARGET','对象已死亡或归档','Target is dead or archived','Ziel ist tot oder archiviert'),('CANCELLED','操作已取消','Operation cancelled','Aktion abgebrochen')]
    errors.append(('DURABILITY_UNKNOWN','存档持久化未确认，当前世界继续保留','Save durability unconfirmed; current world retained','Speicherbeständigkeit unbestätigt; aktuelle Welt bleibt erhalten'))
    defs['Int64Wire']={'type':'string','pattern':'^(0|[1-9][0-9]{0,18})$','maxLength':19}
    defs['PositiveInt64Wire']={'type':'string','pattern':'^[1-9][0-9]{0,18}$','maxLength':19}
    wide_names={'target_revision','expected_target_revision','expected_world_revision','expected_lease_revision','constitution_revision','session_revision','institution_revision','draft_revision','confirmed_slot_revision','quantity','target_quantity','remaining_quantity','amount_minor','fee_minor','budget_minor','wage_minor_per_day','reserve_capacity_units'}
    def wide(s):
        if isinstance(s,dict):
            for name,value in list(s.get('properties',{}).items()):
                if (name in wide_names or name.endswith('_tick')) and value.get('type')=='integer':
                    s['properties'][name]=ref('PositiveInt64Wire' if value.get('minimum',0)>0 else 'Int64Wire')
            for value in s.values():wide(value)
        elif isinstance(s,list):
            for value in s:wide(value)
    wide(defs);wide(schemas);wide(supplementary)
    aliases=runtime['object_ref_wire']['view_aliases_to_canonical_kind']
    target_overrides={
      'RenameEntity':kinds,
      'ProposeStateFoundation':['actor','settlement'],
      'CreateLegalAdoption':['actor','family'],
      'CreatePoliticalPlot':['actor','state'],
      'SubmitPoliticalPetition':['actor','state','political_plot'],
      'AcquireControlLease':['world'],
      'SetMarker':['world','map_marker'],
      'ReorderWorldRuleUI':['world'],
      'CommitVegetationDoseBatch':['world'],
      'EndVegetationStroke':['world'],
      'CommitPeaceTreaty':['state','treaty'],
      'CommitSignedTreaty':['state','treaty'],
    }
    target_contexts={c['id']:target_overrides.get(c['id'],[aliases.get(c.get('target_page','world'),c.get('target_page','world'))]) for c in civic['commands']}
    target_contexts.update({n: (['world'] if n not in ['MoveExistingActorByDivineHand','PreviewObjectDraft','CommitObjectDraft'] else (['actor'] if n=='MoveExistingActorByDivineHand' else kinds)) for n,body,guard in supplementary})
    spec={'schema_version':1,'design_version':'0.9','algorithm_revision':'ADR0018-2026-10-09','planned_only':True,'production_implemented':False,'contract_id':'interaction_runtime_v1','$defs':defs,'pages':pages,'commands':[{'id':c['id'],'payload_schema':schemas[c['id']],'guards_from':'data/contracts/civic_control_v1.json#/commands/'+str(i),'mutates_world':c['mutates_world'],'handler_implemented':False} for i,c in enumerate(civic['commands'])],'supplementary_commands':[{'id':n,'payload_schema':body,'guard':guard,'handler_implemented':False,'single_writer':n!='PreviewObjectDraft'} for n,body,guard in supplementary],'semantic_checks':{'Budget':'sum all bp = 10000; basic living floor protected by economic guards','Education':'learning_bp+practical_bp<=10000; same ActorAvailabilityLedger','Int64Wire':'canonical decimal digits parsed checked 0..9223372036854775807, never JS Number; positive variant excludes0; backend rejects overflow', 'Genome':'unique registered trait_id; alleles are exact raw 0..1000 integers; species immutable; never restore absolute reserves','SubspeciesTraits':'unique registered trait_id; raw profile_trait 0..1000 maps through Life effective_q to fixed species envelope; no advanced_consciousness edits','Ref':'same world/kind/generation; exact live/archive/missing lookup, no name matching','Charter':'unique clause_id and exact legal choices from political definitions','ConstAxes':'exact eight registered axis ids/choices','WorldRuleValue':'per-rule subtype/range from civic world_rules; wrong subtype rejects','Cells':'unique bounded authority cells; never clamp; maximum single prepared stroke 65536 admitted cells, complete closure budget separately checked','Project':'style/definition unlock, real stock/time, whole footprint/access and old certificates revalidated','Licence':'lawful holder/real knowledge, no invented research','oneOf':'exactly one matching strict schema, never first permissive object'},'permission_source':'trusted_dispatcher_session_capability_not_UI_payload; permission_ref lives only in internal envelope','object_draft_commands':['PreviewObjectDraft','CommitObjectDraft'],'object_draft_schema':ref('ObjectDraft'),'object_draft_value_encoding':'typed values use strict per-field union; field must be registered for target kind; duplicate field_id rejects; there is no caller-selected schema or JSON overwrite','errors':[{'code':code,'label':{'zh-CN':zh,'en':en,'de':de}} for code,zh,en,de in errors],'natural_geography_entities':False,'global_tool_routes':[{'tool_id':t['id'],'action':'open_typed_form_or_local_query','target_page':t['primary_page'],'commands':t['route'].get('commands',[]),'handler_implemented':False} for t in tools['tools'] if t['id']!='smooth'],'keyboard':{'map':'WASD_pan_QE_orbit_Space_pause','text_focus':'suppress_map_shortcuts_and_group_numbers_during_IME','escape_order':['close_modal','cancel_preview_or_stroke','release_possession','open_main_menu'],'groups':{'bind':'Ctrl+1..9','select':'1..9'}},'camera':{'yaw_degrees':[0,360],'pitch_degrees':[10,85],'zoom':'distance_new=clamp(distance_old*2^(-wheel_steps/8),profile_min,profile_max)','world_rng':False},'picking':{'ordering':['positive_quantized_ray_distance','Actor','item_equipment','building','Plant','surface','stable_ref'],'filter_tool_target_first':True,'decor_hit':False,'far_selection':'current_layer_actual_ground_point_query','alt':'show_all_permitted_hits'},'input_states':['Observe','BrushArmed','StrokeRecording','PreviewPending','PreviewReady','CommitPending','Meta','Possess','Modal'],'preview_invalidated_by':['target_revision','geometry_revision','binding_container_revision','rule_revision','permission_revision','definition_hash','session_or_draft_generation'],'draft_survives':['navigation','locale_switch','query_refresh'],'production_claim':'schemas and finite reference checks only; no C++/GPU/Steam evidence'}
    spec['command_target_context_kinds']=target_contexts
    spec['envelope_target_guard']='target same World and current generation/revision, kind in per-command target contexts, existing live or permitted local/definition object; creating commands target real parent/initiator. target_page is UI landing, not nonexistent target. ObjectDraft target equals Envelope.target. Dispatcher supplies permission; no caller self-authorization.'
    spec['skill_view_guard']='page_id skill requires view_selector registered skill_id; canonical target ActorRef; Training.actor and skill_id equal view target/selector; non-skill page rejects nonnull selector'
    (ROOT/'data/contracts/interaction_runtime_v1.json').write_text(json.dumps(spec,ensure_ascii=False,indent=2)+'\n')
    print('Generated',len(pages),'pages',sum(len(p['editable_fields']) for p in pages),'field bindings',len(spec['commands']),'command schemas')
if __name__=='__main__': main()
