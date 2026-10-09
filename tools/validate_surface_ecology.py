#!/usr/bin/env python3
"""Validate adopted surface/tree design consistency, not runtime behavior."""
import json
from pathlib import Path

CONTRACT = 'data/contracts/surface_ecology.json'
LOCALES = {'zh-CN', 'en', 'de'}
MEADOW_FRUIT = {'apple', 'pear', 'peach', 'orange', 'mango', 'pomegranate'}
POOLS = {
    'snowfield': {'pine'},
    'flower_meadow': {name + '_tree' for name in MEADOW_FRUIT},
    'maple_field': {'maple'},
    'cherry_field': {'cherry_blossom'},
    'wetland': {'bodhi'},
    'savanna': {'huyang_poplar'},
    'sonnheide_sacred': {'sacred_tree'},
    'volcanic': set(),
    'sand': {'coconut_palm'},
}
THEMES = set(POOLS) - {'sand'}


def require(ok, message):
    if not ok:
        raise ValueError('Surface ecology design: ' + message)


def indexed(items, name):
    result = {item['id']: item for item in items}
    require(len(result) == len(items), 'duplicate ' + name)
    for key, item in result.items():
        require(set(item['labels']) == LOCALES and all(isinstance(v, str) and v.strip() for v in item['labels'].values()), 'trilingual labels: ' + key)
    return result


def flags(data, required_true=(), required_false=()):
    for key in required_true:
        require(data.get(key) is True, key + ' must be true')
    for key in required_false:
        require(data.get(key) is False, key + ' must be false')


def validate_contract(root, c):
    require(c['schema_version'] == 1 and c['contract_id'] == 'sonnheide_layered_surface_v1', 'unknown version')
    flags(c, ('planned_only',), ('production_implemented',))
    require(c['evidence_boundary'] == 'design_registry_validation_not_tree_behavior_or_gpu_evidence', 'false evidence scope')
    for field in ('decision', 'design_base', 'architecture', 'assets'):
        path = (root / c[field]).resolve()
        require(root.resolve() in path.parents and path.is_file(), 'missing/external ' + field)
    layers = c['layers']
    require(set(layers) == {'ground', 'small_plants', 'trees'}, 'three layers missing')
    flags(layers['ground'], (), ('changes_geometry_by_color',))
    flags(layers['small_plants'], ('decorative_only',), ('independent_entity_ids', 'inventory', 'harvest_tasks', 'growth_jobs', 'collision', 'navigation_cost', 'food_or_fiber_output'))
    flags(layers['trees'], ('stable_identity', 'harvestable', 'occupancy_and_clearance'), ('renderer_may_mutate',))
    require(layers['trees']['clock'] == 'SimTick', 'tree clock is not simulation clock')
    profiles = indexed(c['surface_profiles'], 'surface profile')
    require(set(profiles) == set(POOLS) and set(c['theme_ids']) == THEMES and len(c['theme_ids']) == 8, 'eight themes plus sand habitat required')
    for key, profile in profiles.items():
        require(profile['kind'] == ('substrate_habitat' if key == 'sand' else 'soil_theme'), 'sand/theme conflation')
        require(profile['status'] == 'specified_not_implemented' and set(profile['new_tree_pool']) == POOLS[key], 'user tree pool drift: ' + key)
        flags(profile, (), ('paint_resets_existing_trees_or_fruit', 'changes_height_or_water', 'grants_inventory'))
    require(profiles['sand']['decorative_small_plants'] == profiles['volcanic']['decorative_small_plants'] == [], 'sand/volcanic decorate automatically')
    require('clustered_tropical_flowers' in profiles['flower_meadow']['decorative_small_plants'] and profiles['wetland']['decorative_small_plants'] == ['decorative_reeds'] and profiles['savanna']['decorative_small_plants'] == ['decorative_dry_grass'], 'flowers/reeds/dry grass missing')
    species = indexed(c['tree_species'], 'tree species')
    require(set(species) == set().union(*POOLS.values()), 'tree species/pool mismatch')
    fruit = indexed(c['fruit_types'], 'fruit')
    require(set(fruit) == MEADOW_FRUIT | {'coconut'}, 'first fruit samples drift')
    for key, tree in species.items():
        require(tree['status'] == 'planned_original_asset_and_gameplay' and tree['same_tree_identity_across_lod'] and tree['harvestable_timber'], 'false tree implementation or disposable tree identity')
        expected = key[:-5] if key.endswith('_tree') and key[:-5] in MEADOW_FRUIT else ('coconut' if key == 'coconut_palm' else None)
        require(tree['fruit_type'] == expected and tree['fruiting'] is (expected is not None) and (expected is None or expected in fruit), 'fruit species relationship drift')
    for f in fruit.values():
        require(f['food_family'] == 'existing_food' and f['edible_when'] == 'mature_and_actual_harvested_quantity' and not f['per_fruit_entity_required'], 'food/fruit stock authority drift')
        require(len(f['palette_ends']) >= 2 and len(set(f['palette_ends'])) == len(f['palette_ends']), 'fruit maturity has no color change')
    habitat = c['habitat_resolution']
    require(habitat['wet_cell'] == 'no_terrestrial_new_tree' and habitat['dry_sand_priority'] == 'sand', 'water/sand habitat drift')
    flags(habitat, ('paint_sand_does_not_raise_height', 'old_biome_provenance_not_erased_by_sand'), ('sand_has_small_decorations', 'existing_tree_species_auto_swap', 'neutral_ground_is_extra_required_theme'))
    flags(c['agriculture'], ('separate_from_small_surface_plants', 'production_and_fiber_rules_retained', 'farm_batches_not_decoration'))
    edit = c['editing']
    require(edit['writer'] == 'single' and edit['default_new_tree_stage'] == 'young', 'writer/young tree policy drift')
    require(edit['new_tree_initial_harvestable_quantity'] == {'timber': 0, 'fruit': 0} and set(edit['planting_authority_modes']) == {'divine', 'civic', 'wild'}, 'planting grants mature goods or collapses authority')
    flags(edit, (), ('theme_paint_grants_mature_tree_or_fruit', 'template_copies_trees_or_fruit'))
    require({'root_support', 'current_habitat', 'mature_growth_envelope', 'land_use', 'protected_access', 'rights', 'capacity', 'expected_tree_revision'} <= set(edit['planting_revalidates']), 'planting may block existing access')
    require({'id', 'species', 'age', 'woody_biomass', 'fruit_cohorts', 'reservations', 'history'} <= set(edit['existing_tree_fields_preserved']), 'repaint destroys tree authority')
    regrowth = c['regrowth']
    require(regrowth['rule_key'] == 'natural_vegetation_regrowth' and regrowth['controls'] == 'new_wild_tree_candidates_and_spread_only', 'regrowth scope drift')
    flags(regrowth, ('existing_tree_growth_and_fruiting_continue_when_off', 'agriculture_continues_when_off', 'decorative_plants_do_not_enter_rule_scheduler', 'bounded_candidate_queue', 'spawn_identity_and_source_recorded'), ('catch_up_wild_spawns_when_reenabled', 'wall_time_or_visibility_changes_growth'))
    growth = c['fruiting']
    require(growth['model'] == 'bounded_attached_cohorts_per_tree' and growth['stages'] == ['forming', 'growing', 'ripe'] and growth['new_fruit_quantity'] == 'incremental_periodic_with_saved_fractional_work', 'incremental fruit lifecycle missing')
    flags(growth, ('full_tree_stops_new_fruit', 'growing_fruit_not_harvestable', 'ripeness_drives_size_and_color', 'visual_instance_count_derived_from_actual_quantity', 'palette_change_is_not_new_inventory', 'harvest_ripe_quantity_only', 'harvest_requires_actual_path_labor_and_capacity', 'direct_eating_uses_existing_consumption_ledger', 'felling_and_picking_share_tree_revision_and_reservations', 'no_season_or_solar_dependency', 'ripe_fruit_loss_and_ground_recovery_uses_real_quantities', 'quantity_is_absolute_not_maturity_fraction_times_current_yield', 'saved_formed_mature_expiry_ticks', 'cut_pick_same_tree_exclusive_use_permit'), ('repaint_load_lod_resets_fruit', 'fruit_creation_or_harvest_by_particle', 'decoration_flowers_create_fruit'))
    require(growth['stock_transfer'] == 'tree_attachment_to_carrier_or_ground_batch_before_food_consumption', 'fruit teleports to pantry')
    flags(growth, ('last_tick_advances_while_full',), ('full_capacity_accrues_future_formation_credit', 'cohort_cap_merges_distinct_mature_or_expiry_ticks'))
    display = c['presentation']
    flags(display['cherry_petals'], ('decorative_only', 'pause_hide_or_reduce_motion_stops_animation', 'no_actual_fruit_or_wood'), ('persistent_particle_entities', 'world_rng_consumed', 'spawn_by_world_clock_to_change_gameplay', 'lod_culling_changes_tree_stock'))
    flags(display['sacred_glow'], ('emissive_tree_parts', 'bounded_local_tree_lighting', 'steady_soft_bloom_no_strobe', 'low_settings_keep_visible_emissive', 'life_felling_state_controls_emission', 'death_gate_precedes_lod_hysteresis_and_aggregation'), ('light_proxy_lod_changes_tree_state', 'grants_lamp_energy_religion_or_stat_buffs'))
    flags(display['volcanic_lava'], ('default_tree_pool_empty', 'default_small_plant_pool_empty'), ('fluid_simulation', 'world_water_body', 'resource_output', 'damage_or_fire_by_shader', 'automatic_eruption_or_random_disaster'))
    flags(c['persistence'], ('same_tick_save_reload_equals_continuous',), ('load_restocks_or_resurrects',))
    require({'fruit_cohorts_age_quantity_ripeness_and_reservations', 'growth_last_tick_and_fractional_remainders', 'decoration_seed_density_clear_mask'} <= set(c['persistence']['save']), 'saved fruit age/quantity or decoration mask lost')
    inspector = c['tree_inspector']
    require(inspector['domain_kind'] == 'plant' and inspector['subtype'] == 'tree' and inspector['harvest_command'] == 'HarvestPlant' and inspector['harvest_modes'] == ['fruit', 'fell'] and inspector['linked_output_kind'] == 'stock_batch', 'tree inspector domain split')
    flags(inspector, ('quantity_maturity_age_read_only',), ('new_entity_kind', 'decorations_have_entity_page'))
    pixel = json.loads((root / 'data/contracts/pixel_world.json').read_text(encoding='utf-8'))
    require({x['id'] for x in pixel['biomes']} == THEMES and pixel['surface_ecology_extension']['contract'] == CONTRACT, 'pixel registry theme mismatch')
    for b in pixel['biomes']:
        require(b['labels'] == profiles[b['id']]['labels'], 'trilingual theme labels differ')
    tools = {t['id']: t for t in pixel['tools']}
    require(tools['plant_tree']['command'] == 'PlantVegetation' and tools['plant_cover']['command'] == tools['clear_plants']['command'] == 'CommitBiomeStroke', 'decorations routed to economic plant command')
    legacy = json.loads((root / 'data/interaction_schema.json').read_text(encoding='utf-8'))
    require(legacy['active_design_extension']['surface_ecology']['contract'] == CONTRACT, 'inspector extension missing')
    cases = c['acceptance_scenarios']
    require(len(cases) == len(set(cases)) == 15 and {'picking_felling_stale_preview_and_duplicate_receipts', 'road_farm_entrance_growth_exclusion', 'save_at_growth_boundary_camera_independent_replay'} <= set(cases), 'causal acceptance coverage missing')


def validate(root):
    c = json.loads((root / CONTRACT).read_text(encoding='utf-8'))
    validate_contract(root, c)
    return 'ADR0013 design: 3 layers, 8 soil themes + sand habitat, 13 tree samples, 7 fruit samples; not tree/game/GPU validation'


if __name__ == '__main__':
    print('PASS: ' + validate(Path(__file__).resolve().parents[1]))
