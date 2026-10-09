#!/usr/bin/env python3
"""Check the v0.7 design registry, not unimplemented terrain/game behavior."""
import copy
import hashlib
import json
from pathlib import Path

CONTRACT = 'data/contracts/pixel_world.json'
BASELINE_HASH = 'cdff70f277a46554c7d06f0078d1d77049bb88ca965d05e889d619ade949bea6'
LOCALES = {'zh-CN', 'en', 'de'}
PRESETS = {'shallow_water', 'close_ocean', 'deep_ocean', 'sand', 'soil', 'hill', 'mountain', 'peak'}
SECTIONS = {'observe', 'terrain_ecology', 'people', 'civilization', 'construction_resources', 'economy_items', 'world', 'settings'}
NEW_LAWS = {'god_terrain_enabled', 'natural_vegetation_regrowth'}
TEMPLATE_FIELDS = {'heightQ', 'topMaterial', 'biomeId', 'fertility', 'moisture', 'cover'}


def require(ok, message):
    if not ok:
        raise ValueError('Pixel design contract: ' + message)


def read(root, relative):
    path = (root / relative).resolve()
    require(root.resolve() in path.parents and path.is_file(), 'missing/external file: ' + relative)
    return json.loads(path.read_text(encoding='utf-8'))


def labels(items):
    for item in items:
        require(set(item['labels']) == LOCALES and all(isinstance(v, str) and v.strip() for v in item['labels'].values()), 'missing trilingual label: ' + item['id'])


def unique(items, name):
    keys = {item['id'] for item in items}
    require(len(keys) == len(items), 'duplicate ' + name)
    return keys


def validate_contract(root, c):
    require(c['schema_version'] == 1 and c['design_version'] == '0.7', 'unknown contract version')
    require(c['planned_only'] is True and c['production_implemented'] is False, 'design falsely claims game implementation')
    require(c['evidence_boundary'] == 'contract_validation_is_not_game_or_gpu_validation', 'false validation scope')
    source = c['source_baseline']
    require(source['immutable'] is True and source['sha256'] == BASELINE_HASH, 'original source authority changed')
    path = (root / source['path']).resolve()
    require(root.resolve() in path.parents and path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == BASELINE_HASH, 'original source bytes changed')
    for name in [c['decision'], c['design'], *c['contracts']]:
        path = (root / name).resolve()
        require(root.resolve() in path.parents and path.is_file(), 'missing design document: ' + name)
    creation = c['creation']
    require(creation['modes'] == ['blank', 'earth_selection'] and creation['blank_presets'] == ['ocean', 'flat_land'], 'two creation paths missing')
    require(creation['initial_population'] == creation['initial_buildings'] == 0, 'creation grants people/buildings')
    require(not creation['true_dem_required'] and not creation['legacy_world_in_place_rewrite'], 'DEM or unsafe legacy conversion returned')
    require(creation['shared_preview_generator'] and creation['durable_first_checkpoint_before_publish'], 'preview/publication diverges')
    grid = c['grid']
    require(grid['representation'] == 'chunked_single_surface_height_columns' and not grid['dense_xyz_voxels'], 'representation replaced with dense world voxels')
    require(grid['effective_terrain_editable'] and grid['source_provenance_frozen'] and grid['profile_frozen_per_world'], 'source/current terrain authority confused')
    require(grid['mineral_rate_stock_separate'] and grid['navigation_render_collision_share_revision'], 'free mineral or mismatched geometry')
    require(TEMPLATE_FIELDS | {'mutationSource', 'lastEditEvent', 'revision'} == set(grid['authority_fields']), 'terrain fields drift')
    profile = grid['sample_profile']
    require((profile['cell_size_m'], profile['height_quantum_m'], profile['chunk_side_cells']) == (2, .5, 64) and profile['status'] == 'initial_calibration_target_not_measured_capacity', 'sample falsely claims measured capacity')
    water = c['water']
    require(water['sea_level_q'] == water['lake_level_q'] == 0 and water['wet_predicate'] == 'heightQ < 0' and water['connectivity'] == 4, 'water semantics differ')
    require(water['body_ref_stable_with_split_merge_history'] and not water['worker_wall_time_affects_authoritative_ready_tick'], 'water identity/replay depends on worker arrival')
    require(set(water['authoritative_topology_schedule_saved']) == {'logical_frontier', 'consumed_logical_work_budget', 'next_publish_tick', 'input_revision', 'candidate_generation'}, 'pending topology schedule lost on load')
    require(not any(water[k] for k in ('high_altitude_lakes', 'volume_fluid_simulation', 'visual_waves_authoritative')), 'unapproved water scope')
    domain = c['domain']
    require(all(domain[k] for k in ('bounded_authority', 'authority_sea_buffer', 'outside_render_only', 'boundary_visible_in_tool_preview')), 'unbounded or invisible editing domain')
    require(not any(domain[k] for k in ('outer_ocean_guard_editable', 'out_of_bounds_clamp', 'rectangular_world_wall')), 'guard/boundary constraint drift')
    require(unique(c['presets'], 'preset') == PRESETS, 'eight terrain presets missing')
    for p in c['presets']:
        require(p['status'] == 'planned' and p['height_or_depth_is_not_biome'], 'preset status/biome conflation')
        if p['id'] in {'sand', 'soil'}:
            require(p['kind'] == 'material_preset' and p['default_write'] == 'top_material_preserve_height', 'sand/soil silently changes elevation')
        elif p['id'] in {'shallow_water', 'close_ocean', 'deep_ocean'}:
            require(p['kind'] == 'water_depth_preset', 'water depth treated as material/biome')
        else:
            require(p['kind'] == 'landform_preset', 'mountain is not real landform')
    require(len(unique(c['biomes'], 'biome')) == 10 and all(b['status'] == 'planned' and b['paint_grants_goods'] is False for b in c['biomes']), 'biome paint grants inventory')
    editing = c['editing']
    require(editing['modes'] == ['protect', 'destructive'] and editing['writer'] == 'single', 'editing authority drift')
    require(all(editing[k] for k in ('god_events_distinct_from_civic_work', 'destructive_effects_require_contentful_preview', 'immutable_snapshot_preview', 'commit_revalidates_dependencies', 'idempotent_receipt_with_publish', 'terrain_and_immediate_occupant_effects_atomic')), 'unsafe edit commit')
    require(not any(editing[k] for k in ('derived_job_result_may_write_world', 'unconditional_undo', 'occupied_or_economic_effect_undo', 'height_edit_grants_minerals', 'material_edit_grants_goods', 'god_destroy_refunds_consumed_materials', 'template_copies_entities_goods_minerals_or_legal_relations')), 'terrain edit duplicates goods/lives/authority')
    require(set(editing['terrain_template_fields']) == TEMPLATE_FIELDS, 'clipboard includes non-terrain authority')
    ports = c['ports']
    require(ports['allowed_dry_origins'] == ['INITIAL', 'DIVINE_EDIT', 'CIVIC_EARTHWORK'] and not ports['reclaimed_only'], 'legacy reclaimed-only ports returned')
    require(ports['rotations'] == [0, 90, 180, 270] and all(ports[k] for k in ('full_support_required', 'depth_clearance_approach_required', 'divine_edit_may_invalidate', 'ordinary_construction_preserves_access')), 'port geometry/access drift')
    people = c['characters']
    require(people['source'] == 'original_cuboid_mesh_pixel_texture' and not people['makehuman_production_dependency'] and not people['billboard_final_character'] and not people['full_screen_pixel_filter_as_art'], 'pixel characters replaced with old source/filter')
    require(people['same_body_dimensions_for_all_ages'] and people['player_placed_age'] == 18 and people['birth_age'] == 0 and not people['auto_spawn_migrants_workers_soldiers'], 'population/body contract drift')
    require(people['clothes_are_real_goods'] and not people['formalwear_required_to_work'] and people['uniform_color_binding'] == 'actual_serving_state', 'clothing economy drift')
    display = c['presentation']
    require(display['true_3d'] and display['yaw_degrees'] == 360 and display['pixel_water_default'] and display['main_menu_oil_paintings_retained'], '3D/pixel water/approved menu lost')
    require(not any(display[k] for k in ('buttons_have_borders', 'unnecessary_small_text', 'poly_haven_hex_required_for_new_world', 'abyssal_native_port_mandatory', 'astronomical_day_night')) and display['platform_store'] == 'steam_only' and display['ages'] == ['light', 'darkness'], 'visual/store scope changed')
    require(unique(c['sections'], 'section') == SECTIONS, 'eight bottom sections missing')
    commands = {x['id']: x for x in c['commands']}
    require(len(commands) == len(c['commands']) == 23, 'duplicate/missing command')
    for name, command in commands.items():
        require(command['status'] == 'planned_pixel_integration' and command['trusted_authority'] == 'dispatcher' and command['preserve_conservation'], 'false/untrusted command: ' + name)
        require(command['preview_only'] == name.startswith('Preview') and command['world_writer'] is (not command['preview_only']), 'preview writes World: ' + name)
    tools = c['tools']
    ids = unique(tools, 'tool')
    require(len(ids) == 66 and {'terrain_' + key for key in PRESETS} <= ids, 'tool/preset coverage drift')
    for tool in tools:
        require(tool['section'] in SECTIONS and tool['status'] == 'planned_pixel_route', 'tool section or false implementation')
        require(tool['mode'] in {'view', 'application', 'world_preview'}, 'tool authority mode')
        if tool['mode'] == 'world_preview':
            require(tool.get('command') in commands and commands[tool['command']]['world_writer'], 'tool bypasses typed writer')
        else:
            require(not tool.get('command'), 'view/application tool mutates World')
    labels(c['presets'] + c['biomes'] + c['sections'] + tools)
    legacy = read(root, 'data/interaction_schema.json')
    ext = legacy['active_design_extension']
    require(ext['contract'] == CONTRACT and ext['decision'] == c['decision'] and ext['status'] == 'planned_only' and ext['legacy_oracle_and_browser_fixture_unchanged'], 'legacy fixture/new design not separated')
    require(set(c['world_laws']) == set(legacy['world_rules']['allowed_keys']) | NEW_LAWS and len(c['world_laws']) == 6 and c['world_rule_command'] == legacy['world_rules']['command'] == 'SetWorldRule', 'World Laws old/new command drift')
    inspectors = c['inspectors']
    require(inspectors['base'] == 'data/interaction_schema.json' and inspectors['new_refs'] == ['TerrainCellRef', 'WaterBodyRef'] and inspectors['typed_relations'] and not inspectors['ui_locale_affects_world_language'] and inspectors['former_frozen_terrain_fields_superseded'], 'inspector routing drift')
    source_chapters = read(root, 'data/requirements/chapters.json')['chapters']
    reviews = c['chapter_review']
    require([r['chapter'] for r in reviews] == list(range(1, 28)), 'chapter missing/duplicate')
    for r, original in zip(reviews, source_chapters):
        require(r['source_title'] == original['title'] and r['decision'] and r['status'] == 'reviewed_design_production_pending', 'source trace or false chapter implementation')
    scenarios = c['acceptance_scenarios']
    require(len(scenarios) == len(set(scenarios)) == 21 and {'sea_channel_split_merge_remote_ports', 'occupied_building_interior_residents', 'stranded_boat_army_passengers_cargo', 'old_save_explicit_copy_migration', 'camera_independent_simulation_replay'} <= set(scenarios), 'causal acceptance missing')


def validate(root):
    c = read(root, CONTRACT)
    validate_contract(root, c)
    return 'v0.7 planned design: 8 presets, 10 biomes, 8 sections, 66 trilingual tools, 23 typed intents, 6 World Laws, 27 chapter reviews; not game/GPU validation'


if __name__ == '__main__':
    print('PASS: ' + validate(Path(__file__).resolve().parents[1]))
