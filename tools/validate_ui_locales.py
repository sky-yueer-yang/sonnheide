#!/usr/bin/env python3
"""Check offline UI message identity, coverage and interpolation using stdlib only."""
import json
from pathlib import Path
import re


def require(condition, message):
    if not condition:
        raise ValueError('UI locales: ' + message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key ' + key)
        result[key] = value
    return result


def load_json(text):
    return json.loads(text, object_pairs_hook=unique_object)


def validate(root):
    registry = load_json((root / 'data/ui_locales.json').read_text(encoding='utf-8'))
    require(registry['schema_version'] == 1, 'unsupported version')
    require(registry['default_locale'] == 'zh', 'default interface locale drift')
    require(set(registry['supported_locales']) == {'zh', 'en', 'de'} and len(registry['supported_locales']) == 3,
            'exactly Chinese, English and German are required')
    require(registry['scope'] == 'interface_only_not_simulation_language', 'locale changes simulation language')
    messages = registry['messages']
    require(set(messages) == {'zh', 'en', 'de'}, 'missing/extra locale catalog')
    keys = set(messages['zh'])
    require(keys, 'empty message catalog')
    for locale, catalog in messages.items():
        require(set(catalog) == keys, 'message key parity failed: ' + locale)
        for key, value in catalog.items():
            require(isinstance(value, str) and value.strip(), 'empty/non-text message: ' + locale + '/' + key)
            require(re.fullmatch(r'[A-Za-z0-9_.-]+', key) is not None, 'unstable message key: ' + key)
            expected = set(re.findall(r'\{([A-Za-z_][A-Za-z0-9_]*)\}', messages['zh'][key]))
            require(set(re.findall(r'\{([A-Za-z_][A-Za-z0-9_]*)\}', value)) == expected,
                    'interpolation parameters differ: ' + locale + '/' + key)
    interaction = load_json((root / 'data/interaction_schema.json').read_text(encoding='utf-8'))
    laws = load_json((root / 'data/catalogs/laws.json').read_text(encoding='utf-8'))['laws']
    required = {'kind.' + entity['kind'] for entity in interaction['entities']}
    required.update('law.' + law['id'] for law in laws)
    required.update('trait.' + axis for axis in interaction['editing_contract']['personality']['axes'])
    required.update('section.' + section for section in
                    ('observe', 'people', 'civilization', 'construction', 'economy', 'world', 'settings'))
    require(required <= keys, 'missing kind/law/personality/section messages: ' + ', '.join(sorted(required - keys)))
    html = (root / 'tools/previews/world-inspector.html').read_text(encoding='utf-8')
    embedded = re.search(r'<script\b[^>]*\bid=[\"\']world-inspector-locales[\"\'][^>]*>(.*?)</script>', html, re.S)
    require(embedded is not None, 'offline message mirror missing')
    require(load_json(embedded.group(1)) == messages, 'offline HTML mirror differs from authored JSON')
    return '{} messages in zh/en/de, complete kind/law/axis/section keys, interpolation parity and offline mirror'.format(len(keys))


if __name__ == '__main__':
    print('PASS: ' + validate(Path(__file__).resolve().parents[1]))
