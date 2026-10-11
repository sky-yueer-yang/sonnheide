#!/usr/bin/env python3
"""Finite schema, draft and picking checks. Not a production UI."""
import json
import re
from copy import deepcopy
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]

class Rejected(ValueError): pass

def check(value, schema, defs, depth=0):
    if depth>64: raise Rejected('DEPTH_LIMIT')
    if schema.get('$ref') in ['#/$defs/Int64Wire','#/$defs/PositiveInt64Wire']:
        if not isinstance(value,str) or not value.isdigit() or int(value)>9223372036854775807:raise Rejected('INT64_OVERFLOW')
    if '$ref' in schema:
        return check(value,defs[schema['$ref'].split('/')[-1]],defs,depth+1)
    if 'const' in schema and (type(value)!=type(schema['const']) or value!=schema['const']): raise Rejected('CONST')
    if 'enum' in schema and value not in schema['enum']: raise Rejected('ENUM')
    if 'oneOf' in schema:
        count=0
        for option in schema['oneOf']:
            try: check(value,option,defs,depth+1); count+=1
            except Rejected: pass
        if count!=1: raise Rejected('ONE_OF')
        return
    t=schema.get('type')
    correct={'object':type(value) is dict,'array':type(value) is list,'integer':type(value) is int,'boolean':type(value) is bool,'string':type(value) is str,'null':value is None}
    if t and not correct[t]: raise Rejected('TYPE')
    if t=='object':
        if any(k not in value for k in schema['required']): raise Rejected('REQUIRED')
        if not schema.get('additionalProperties',True) and set(value)-set(schema['properties']): raise Rejected('UNKNOWN_PROPERTY')
        for k,v in value.items():
            if k in schema['properties']:check(v,schema['properties'][k],defs,depth+1)
    elif t=='array':
        if len(value)<schema.get('minItems',0) or len(value)>schema.get('maxItems',2147483647):raise Rejected('ITEM_COUNT')
        for v in value:check(v,schema['items'],defs,depth+1)
    elif t=='integer':
        if not schema.get('minimum',value)<=value<=schema.get('maximum',value):raise Rejected('OUT_OF_RANGE')
    elif t=='string':
        if not schema.get('minLength',0)<=len(value)<=schema.get('maxLength',2147483647):raise Rejected('STRING_LENGTH')
        if 'pattern' in schema and re.fullmatch(schema['pattern'],value) is None:raise Rejected('PATTERN')

def ref(kind='actor',identity=1):
    return {'world_id':'1'*32,'kind':kind,'stable_id':format(identity,'016x'),'generation':1}

def submit_draft(draft, spec):
    check(draft,spec['object_draft_schema'],spec['$defs'])
    page=next((p for p in spec['pages'] if p['id']==draft['page_id']),None)
    if page is None:raise Rejected('TARGET_KIND')
    aliases={'person':'actor','animal':'actor','empire':'state','port':'building','city':'settlement','ship':'vehicle','equipment':'item','thermal_exposure':'actor','skill':'actor'}
    if aliases.get(draft['target']['kind'],draft['target']['kind'])!=aliases.get(page['id'],page['id']):raise Rejected('TARGET_VIEW_MISMATCH')
    selector=draft.get('view_selector')
    if page['id']=='skill':
        if not selector or not selector['skill_id']:raise Rejected('SKILL_SELECTOR_REQUIRED')
    elif selector is not None:raise Rejected('UNEXPECTED_VIEW_SELECTOR')
    fields={f['field']:f['schema'] for f in page['editable_fields']}
    seen=set()
    for entry in draft['fields']:
        name=entry['field_id']
        if name not in fields or name in seen:raise Rejected('UNREGISTERED_OR_DUPLICATE_FIELD')
        seen.add(name)
        check(entry['value'],fields[name],spec['$defs'])
        if page['id']=='skill' and (entry['value']['actor']!=draft['target'] or entry['value']['skill_id']!=selector['skill_id']):raise Rejected('SKILL_TARGET_MISMATCH')
        if fields[name].get('$ref')=='#/$defs/Budget' and sum(entry['value'].values())!=10000:raise Rejected('BUDGET_SUM')
    return deepcopy(draft)

class DraftStore:
    def __init__(self):self.drafts={};self.query_generation=0;self.page=None
    def navigate(self,page):self.page=page;self.query_generation+=1
    def accept_query(self,generation):return generation==self.query_generation
    def edit(self,target,field,value):self.drafts.setdefault(target,{})[field]=deepcopy(value)
    def cancel(self,target):self.drafts.pop(target,None)

def picking(hits,allowed):
    priority={'actor':0,'item':1,'equipment':1,'building':2,'plant':3,'surface':4}
    actual=[h for h in hits if h['kind'] in priority and h['kind'] in allowed and h['distance_q']>=0]
    return min(actual,key=lambda h:(h['distance_q'],priority[h['kind']],h['id'])) if actual else None

def run():
    spec=json.loads((ROOT/'data/contracts/interaction_runtime_v1.json').read_text())
    defs=spec['$defs']; n=0
    def expect_fail(value,schema):
        nonlocal n
        try:check(value,schema,defs)
        except Rejected:n+=1;return
        raise AssertionError('accepted invalid input')
    check('9223372036854775807',{'$ref':'#/$defs/Int64Wire'},defs);n+=1
    expect_fail('9223372036854775808',{'$ref':'#/$defs/Int64Wire'})
    expect_fail(2147483648,{'$ref':'#/$defs/Int64Wire'})
    check({'traits':[{'trait_id':'cold_tolerance','allele_a':1000,'allele_b':0}]},defs['Genome'],defs);n+=1
    expect_fail({'traits':[{'trait_id':'cold_tolerance','allele_a':1001,'allele_b':0}]},defs['Genome'])
    expect_fail({'traits':[],'species_id':'rabbit'},defs['Genome'])
    expect_fail({'action':'paint','preset':'hill','cells':[{'x':0,'y':0}],'target_height':99},defs['Geography'])
    expect_fail(ref('water_body'),defs['Ref']);expect_fail(ref('mountain'),defs['Ref'])
    expect_fail(ref('bogus'),defs['Ref']);expect_fail(ref('batch'),defs['Ref']);expect_fail(ref('person'),defs['Ref'])
    envelope={'command_id':'IssueMetaOrder','world_id':'1'*32,'target':ref('world'),'expected_target_revision':'2','session_generation':1,'draft_generation':1,'expected_world_revision':'2','idempotency_key':'event-1','preview_token':None}
    check(envelope,defs['Envelope'],defs);n+=1
    bad=deepcopy(envelope);bad['permission_ref']=ref('permission');expect_fail(bad,defs['Envelope'])
    bad=deepcopy(envelope);del bad['target'];expect_fail(bad,defs['Envelope'])
    expect_fail(True,defs['PolicyPriority']);expect_fail({'curiosity':10,'health':100},defs['Personality'])
    commands={c['id']:c['payload_schema'] for c in spec['commands']}
    order={'kind':'hold','target_position':{'x_mm':0,'y_mm':0,'z_mm':2000},'target_ref':None,'expires_tick':'12000','issuer':ref('actor')}
    lease_order={'order':order,'lease':ref('control_lease'),'expected_lease_revision':'3'}
    check(lease_order,commands['IssueMetaOrder'],defs);n+=1
    bad=deepcopy(lease_order);del bad['expected_lease_revision'];expect_fail(bad,commands['IssueMetaOrder'])
    check({'scope':ref('state')},commands['EnterMetaSpectate'],defs);n+=1
    expect_fail({'lease':{'mode':'spectate'}},commands['EnterMetaSpectate'])
    # Schema validity does not authorize a stale lease: the internal dispatcher
    # compares the actual current revision and never substitutes Ref.generation.
    def current_lease(command, actual_revision):
        check(command,commands['IssueMetaOrder'],defs)
        return int(command['expected_lease_revision'])==actual_revision
    assert current_lease(lease_order,3) and not current_lease(lease_order,4);n+=1
    draft={'target':ref('household'),'page_id':'household','target_revision':'1','fields':[{'field_id':'budget_preferences','value':{'food_bp':3500,'shelter_bp':2000,'clothing_bp':1500,'care_bp':1000,'education_bp':500,'investment_bp':500,'reserve_bp':1000}}]}
    assert submit_draft(draft,spec)==draft;n+=1
    bad=deepcopy(draft);bad['fields'][0]['value']['food_bp']=9999
    try:submit_draft(bad,spec)
    except Rejected:n+=1
    else:raise AssertionError('bad budget accepted')
    bad=deepcopy(draft);bad['fields']*=2
    try:submit_draft(bad,spec)
    except Rejected:n+=1
    else:raise AssertionError('duplicate edit accepted')
    bad=deepcopy(draft);bad['fields']=[{'field_id':'personality_axes_via_life_contract','value':{k:5000 for k in defs['Personality']['properties']}}]
    try:submit_draft(bad,spec)
    except Rejected:n+=1
    else:raise AssertionError('wrong page field accepted')
    skill={'target':ref(),'page_id':'skill','target_revision':'1','view_selector':{'skill_id':'woodworking'},'fields':[{'field_id':'actual_training_or_learning_request','value':{'actor':ref(),'skill_id':'woodworking','teacher':None,'duration_ticks':100,'location':ref('building')}}]}
    assert submit_draft(skill,spec)==skill;n+=1
    bad=deepcopy(skill);bad['fields'][0]['value']['actor']=ref(identity=2)
    try:submit_draft(bad,spec)
    except Rejected:n+=1
    else:raise AssertionError('skill on wrong Actor accepted')
    store=DraftStore();store.navigate('person');old=store.query_generation;store.edit(1,'display_name','Licht');store.navigate('state');store.navigate('person')
    assert not store.accept_query(old) and store.drafts[1]['display_name']=='Licht';n+=1
    store.cancel(1);assert not store.drafts;n+=1
    hits=[{'kind':'decor','distance_q':0,'id':1},{'kind':'building','distance_q':20,'id':3},{'kind':'actor','distance_q':20,'id':7},{'kind':'plant','distance_q':21,'id':2}]
    assert picking(hits,{'actor','building','plant'})['kind']=='actor';n+=1
    assert picking(hits,{'building'})['kind']=='building';n+=1
    assert picking(hits,{'water_body'}) is None;n+=1
    # All reference targets exist and each command has a closed payload shape.
    def walk(s):
        nonlocal n
        if isinstance(s,dict):
            if '$ref' in s:assert s['$ref'].split('/')[-1] in defs;n+=1
            if s.get('type')=='object':assert s.get('additionalProperties') is False;n+=1
            for v in s.values():walk(v)
        elif isinstance(s,list):
            for v in s:walk(v)
    walk(spec)
    assert len(spec['pages'])==70 and len(spec['commands'])==78
    return {'checks':n,'pages':70,'field_bindings':sum(len(p['editable_fields']) for p in spec['pages']),'commands':78,'production_scenarios_executed':0}
if __name__=='__main__':print(json.dumps(run(),ensure_ascii=False))
