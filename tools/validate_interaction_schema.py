#!/usr/bin/env python3
"""Validate the additional registry without altering immutable source catalogs."""
import json
from pathlib import Path
import re

EXPECTED_KINDS = set('person household settlement city state empire alliance culture language religion denomination church enterprise building port army vehicle garment stock_batch contract project innovation document plant mineral_deposit road world map_marker account reservation shipment production_line facility knowledge_license research_project war treaty genesis_node name_record history_event'.split())
EXPECTED_RULES = {'reproduction', 'aging', 'hunger_consequences', 'ai_new_wars'}


def require(condition, message):
    if not condition:
        raise ValueError('Interaction contract: ' + message)


def validate(root):
    schema = json.loads((root / 'data/interaction_schema.json').read_text(encoding='utf-8'))
    require(schema['schema_version'] == 1 and schema['planned_only'] is True, 'false production status')
    identity = schema['identity_contract']
    require(identity['reference_fields'] == ['world_id', 'kind', 'id'], 'typed reference shape changed')
    require(identity['never_reuse_ids'] and identity['names_are_not_identity'] and identity['cross_world_refs'] == 'reject', 'unsafe identity contract')
    entities = schema['entities']
    kinds = {e['kind'] for e in entities}
    require(len(entities) == len(kinds) == 40 and kinds == EXPECTED_KINDS, 'missing/duplicate inspector kinds')
    sections = {s['id'] for s in schema['inspector_sections']}
    require(sections == {'overview', 'relations', 'assets_tasks', 'statistics', 'history', 'edit'}, 'incomplete page topology')
    commands = schema['command_contract']['registry']
    names = {c['kind'] for c in commands}
    require(len(commands) == len(names), 'duplicate commands')
    require(not schema['command_contract']['generic_json_overwrite'] and not schema['command_contract']['trusted_authority_from_payload'], 'unrestricted write/authority')
    for command in commands:
        require(command['implementation_status'] == 'specified_not_production', 'command falsely claimed implemented')
        require(command['scope'] in {'world_transaction', 'player_profile', 'read_only'}, 'unknown command scope')
        require(command['payload_type'] and command['authority_source'], 'missing typed dispatch')
    relation_count = 0
    for entity in entities:
        require(set(entity['sections']) == sections and len(entity['summary_fields']) <= 4, 'page contract mismatch: ' + entity['kind'])
        require(entity['relations'], 'dead-end inspector: ' + entity['kind'])
        keys = set()
        for relation in entity['relations']:
            require(relation['key'] not in keys and relation['target_kinds'], 'duplicate/empty relation')
            keys.add(relation['key'])
            require(set(relation['target_kinds']) <= kinds, 'unregistered relation destination')
            require(relation['identity'] == 'typed_stable_ref' and relation['query'] == 'indexed_read_only_snapshot', 'unsafe relationship query')
            relation_count += 1
        fields = set()
        for edit in entity['free_edits']:
            require(edit['field'] not in fields and edit['field'] not in entity['read_only'], 'editable read-only/duplicate field')
            fields.add(edit['field'])
            require(edit['command'] in names and edit['policy'], 'unregistered field edit')
        for edit in entity['procedural_edits']:
            require(edit['command'] in names and edit['policy'], 'unregistered procedural action')
        require(entity['history_ref_kind'] == 'history_event', 'history route missing')
    rules = schema['world_rules']
    require(set(rules['allowed_keys']) == EXPECTED_RULES and set(rules['defaults']) == EXPECTED_RULES, 'world law mismatch')
    require({r['key'] for r in rules['rules']} == EXPECTED_RULES and all(type(v) is bool for v in rules['defaults'].values()), 'invalid world laws')
    population = schema['population_rules']
    require(population['allowed_creation_sources'] == ['player_placement', 'birth'] and population['initial_world_population'] == 0, 'implicit population spawn')
    require(population['placement']['initial_age_years'] == 18 and population['birth']['initial_age_years'] == 0, 'incorrect initial ages')
    require(not population['placement']['automatic_grants'], 'placement creates economic assets')
    body = population['body_spec']
    require(all(body[k] is True for k in ('uniform_scale', 'uniform_skeleton_interface', 'uniform_collision_and_portal_clearance', 'uniform_garment_fit', 'simulation_eligibility_still_age_based')), 'age/body coupling')
    require(all(body[k] is False for k in ('age_dependent_mesh_scale', 'child_body_or_rig', 'growth_size_purchase')), 'child/age size profiles returned')
    require(schema['statistics_contract']['unavailable_is_zero'] is False, 'missing statistics fabricated as zero')
    require(set(schema['clear_tools']['categories']) == {'trees', 'plants', 'mineral_resources', 'buildings', 'roads', 'people'}, 'unsafe clear mask')
    # The code registry is checked for missing kinds; behavioral safety is exercised by C++/JS tests.
    code = (root / 'engine/src/interaction.cpp').read_text(encoding='utf-8')
    array = re.search(r'names\s*\{(.*?)\};', code, re.S)
    require(array is not None and set(re.findall(r'"([a-z_]+)"', array.group(1))) == kinds, 'C++ inspector registry drift')
    for path in ('docs/architecture/CONTENT_PIPELINE.md', 'docs/architecture/CLOTHING_ECONOMY.md', 'docs/research/MAKEHUMAN_ECOSYSTEM.md'):
        content = (root / path).read_text(encoding='utf-8')
        require(not any(old in content for old in ('成人、儿童比例分别制作', '儿童使用独立、适合年龄', '成人/儿童各有正确比例', '不共用同一组骨长度')), 'obsolete body requirement: ' + path)
    return '40 typed inspector kinds, {} registered relation routes, {} specified command intents, population/body/rules/clear contracts'.format(relation_count, len(commands))


if __name__ == '__main__':
    print('PASS: ' + validate(Path(__file__).resolve().parents[1]))
