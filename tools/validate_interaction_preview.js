#!/usr/bin/env node
"use strict";
/* Execute the shipped HTML's actual model, without a browser or dependencies. */
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const root = path.resolve(__dirname, "..");
const html = fs.readFileSync(path.join(root, "tools/previews/world-inspector.html"), "utf8");
const extract = id => {
  const match = html.match(new RegExp('<script id="' + id + '">([\\s\\S]*?)<\\/script>'));
  assert.ok(match, "Missing executable HTML script: " + id);
  return match[1];
};
const context = vm.createContext({});
vm.runInContext(extract("world-inspector-model"), context, {timeout: 3000});
new vm.Script(extract("world-inspector-ui"), {filename: "world-inspector-ui.js"});
const M = context.SonnheideInspectorModel;
const plain = value => JSON.parse(JSON.stringify(value));
const worldSnapshot = store => JSON.stringify(store.snapshot().world);
let count = 0;
function test(name, action) {
  action();
  count++;
  process.stdout.write("PASS " + name + "\n");
}
function fresh() { return M.createStore(); }
function expectUnchanged(store, action, pattern) {
  const before = worldSnapshot(store);
  assert.throws(action, pattern);
  assert.equal(worldSnapshot(store), before, "Rejected command changed world state");
}

test("40 kinds agree with the machine contract and all fixture references resolve", () => {
  const schema = JSON.parse(fs.readFileSync(path.join(root, "data/interaction_schema.json"), "utf8"));
  assert.ok(Array.isArray(schema.entities), "Contract entities must be an array");
  const kinds = schema.entities.map(entity => entity.kind);
  assert.equal(new Set(kinds).size, 40);
  assert.deepEqual(plain(M.KINDS).sort(), kinds.sort());
  const store = fresh();
  assert.deepEqual(plain(M.INSPECTOR_SECTIONS), schema.inspector_sections.map(section => section.id));
  assert.deepEqual(plain(store.rules()), schema.world_rules.defaults);
  assert.deepEqual(plain(store.clearKinds()), schema.clear_tools.categories);
  const capabilities = plain(store.nameCapabilities());
  assert.deepEqual(capabilities.rename.sort(), schema.entities.filter(e => e.free_edits.some(edit => edit.command === "RenameEntity")).map(e => e.kind).sort(), "Rename capability must match contract policy");
  assert.deepEqual(capabilities.alias.sort(), schema.entities.filter(e => e.free_edits.some(edit => edit.command === "SetDisplayAlias")).map(e => e.kind).sort(), "Display aliases must remain separate from RenameEntity");
  assert.deepEqual(plain(store.inspectReferences()), []);
  const ids = new Set();
  store.all().forEach(object => {
    assert.ok(!ids.has(object.id), "Stable IDs must not be reused across kinds");
    ids.add(object.id);
    assert.equal(object.stableId, object.id);
    assert.ok(schema.identity_contract.lifecycle.includes(object.status));
    assert.equal(store.resolve(store.ref(object)).kind, object.kind);
  });
  kinds.forEach(kind => assert.ok(store.all().some(o => o.kind === kind && o.name && o.relations.length), "Missing populated and connected kind: " + kind));
});

test("person nationality/employer/household/language navigation and back/forward are read-only", () => {
  const store = fresh(), refs = store.fixtureRefs(), before = worldSnapshot(store);
  store.navigate(refs.person);
  const person = store.resolve(refs.person);
  [["法定国籍", "state"], ["企业 / 雇主", "enterprise"], ["家庭", "household"], ["语言", "language"]].forEach(([label, kind]) => {
    const relation = person.relations.find(r => r.label === label);
    assert.equal(store.resolve(relation.ref).kind, kind);
    store.navigate(relation.ref);
    assert.ok(M.sameRef(store.current(), relation.ref));
    assert.ok(M.sameRef(store.back(), refs.person));
    assert.ok(M.sameRef(store.forward(), relation.ref));
    store.back();
  });
  assert.equal(worldSnapshot(store), before);
  const navBefore = JSON.stringify(store.navigation());
  assert.throws(() => store.navigate({...plain(refs.state), world_id: "different-world"}), /跨世界/);
  assert.equal(JSON.stringify(store.navigation()), navBefore);
  assert.throws(() => store.resolve({...plain(refs.person), kind: "state"}), /kind/);
});

test("camera, favorites, marker creation and notes do not stale a world edit", () => {
  const store = fresh(), refs = store.fixtureRefs(), draft = store.beginEdit(refs.person), before = worldSnapshot(store);
  store.setCameraMode("far");
  store.toggleFavorite(refs.person);
  store.toggleFavorite(refs.garment);
  const marker = store.createMarker({x: 250, y: 140}, "观察点", refs.person);
  assert.equal(store.resolve(marker).kind, "map_marker");
  assert.ok(store.isFavorite(marker));
  const note = store.beginNote(refs.account);
  note.text = "待核对材料账户";
  store.commitNote(note);
  assert.equal(store.note(refs.account), note.text);
  assert.equal(worldSnapshot(store), before);
  draft.fields.name = "林澄新名";
  store.commitEdit(draft);
  assert.equal(store.resolve(refs.person).name, "林澄新名");
  assert.deepEqual(plain(store.inspectReferences()), []);
});

test("player placement is always age 18 and creates unique provenance records", () => {
  const store = fresh();
  const refs = [store.createPlayerPerson({x: 200, y: 180}, "甲", {age: 0}), store.createPlayerPerson({x: 210, y: 190}, "乙", {age: 80})];
  assert.notEqual(refs[0].id, refs[1].id);
  refs.forEach(ref => {
    const person = store.resolve(ref);
    assert.equal(person.age, 18);
    assert.equal(person.fields.身体规格, "统一成年体型 / 同尺度");
    assert.equal(store.resolve(person.history[0].ref).eventKind, "player_placement");
    assert.ok(!person.relations.some(r => ["state", "culture", "language", "enterprise", "household"].includes(r.ref.kind)), "Placement must not grant identity or employment");
  });
  assert.equal(store.statistics(store.worldRef()).living, 6);
  assert.equal(store.statistics(store.fixtureRefs().state).living, 4, "Placement does not create new nationals");
});

test("placement rejects ocean/out-of-bounds/capacity atomically and never clamps coordinates", () => {
  const store = fresh();
  [{x: 10, y: 10}, {x: 620, y: 400}, {x: -1, y: 180}, {x: 200, y: Infinity}].forEach(position => expectUnchanged(store, () => store.createPlayerPerson(position), /LAND/));
  const position = {x: 215.25, y: 190.75};
  const first = store.createPlayerPerson(position);
  assert.deepEqual(plain(store.resolve(first).position), position);
  while (store.statistics(store.worldRef()).living < 32) store.createPlayerPerson({x: 225, y: 190});
  expectUnchanged(store, () => store.createPlayerPerson({x: 225, y: 190}), /容量/);
});

test("birth requires one explicit paired-adult source event and retries are idempotent", () => {
  const store = fresh(), refs = store.fixtureRefs();
  expectUnchanged(store, () => store.applyReproductionEvent({source_event_id: "arbitrary-baby", parents: [refs.mother, refs.father]}), /来源/);
  expectUnchanged(store, () => store.applyReproductionEvent({source_event_id: "demo-family-birth-001", parents: [refs.person, refs.father]}), /来源/);
  const event = {source_event_id: "demo-family-birth-001", parents: [refs.mother, refs.father]};
  const child = store.applyReproductionEvent(event);
  assert.equal(store.resolve(child).age, 0);
  assert.equal(store.resolve(child).fields.身体规格, store.resolve(refs.person).fields.身体规格);
  assert.equal(store.resolve(store.resolve(child).birthEvent).sourceEventId, event.source_event_id);
  const before = worldSnapshot(store);
  assert.ok(M.sameRef(store.applyReproductionEvent(event), child));
  assert.equal(worldSnapshot(store), before);
  expectUnchanged(store, () => store.applyReproductionEvent({source_event_id: event.source_event_id, parents: [refs.father, refs.mother]}), /来源/);
  assert.equal(store.statistics(refs.household).children, 1);
  assert.deepEqual(plain(store.inspectReferences()), []);
});

test("birth rejects underage, non-partner or deceased parents without side effects", () => {
  ["underage", "nonpartner", "deceased"].forEach(mode => {
    const fixture = M.createFixture(), refs = fixture.fixtureRefs;
    if (mode === "underage") fixture.objects[refs.mother.id].age = 17;
    if (mode === "nonpartner") fixture.objects[refs.mother.id].partner = refs.person;
    const store = M.createStore(fixture);
    if (mode === "deceased") store.commitClear(store.previewClear(["person"], [refs.mother]).token);
    expectUnchanged(store, () => store.applyReproductionEvent({source_event_id: "demo-family-birth-001", parents: [refs.mother, refs.father]}), /成年双亲/);
  });
});

test("reproduction off blocks birth while permitting fixed-age player placement", () => {
  const store = fresh(), refs = store.fixtureRefs();
  store.setWorldLaw("reproduction", false, store.revisions().simRevision);
  expectUnchanged(store, () => store.applyReproductionEvent({source_event_id: "demo-family-birth-001", parents: [refs.mother, refs.father]}), /繁衍已关闭/);
  assert.equal(store.resolve(store.createPlayerPerson({x: 250, y: 180})).age, 18);
});

test("world laws change revision and history without inventing food or deleting war records", () => {
  const store = fresh(), refs = store.fixtureRefs(), before = store.resolve(refs.person).fields.食物单位;
  const stock = plain(store.resolve(refs.stock_batch)), war = plain(store.resolve(refs.war));
  const revision = store.revisions().simRevision;
  store.setWorldLaw("hunger_consequences", false, revision);
  assert.equal(store.revisions().simRevision, revision + 1);
  assert.equal(store.resolve(refs.person).fields.食物单位, before);
  assert.deepEqual(plain(store.resolve(refs.stock_batch)), stock);
  store.setWorldLaw("ai_new_wars", false, store.revisions().simRevision);
  assert.deepEqual(plain(store.resolve(refs.war)), war);
  assert.ok(store.resolve(refs.world).history.some(h => store.resolve(h.ref).eventKind === "world_law"));
  expectUnchanged(store, () => store.setWorldLaw("world_scale", false), /规则无效/);
});

test("name/personality edits are staged, emit history, retain typed IDs and reject stale drafts", () => {
  const store = fresh(), refs = store.fixtureRefs(), draft = store.beginEdit(refs.person), before = worldSnapshot(store);
  draft.fields.name = '<img src=x onerror="alert(1)">';
  draft.fields.personality = "耐心";
  assert.equal(worldSnapshot(store), before);
  store.commitEdit(draft);
  const person = store.resolve(refs.person);
  assert.equal(person.name, draft.fields.name);
  assert.equal(person.personality, "耐心");
  assert.equal(person.id, refs.person.id);
  assert.ok(person.relations.some(r => r.ref.kind === "name_record"));
  assert.ok(person.history.some(h => store.resolve(h.ref).eventKind === "edit"));
  const stale = store.beginEdit(refs.state);
  store.createPlayerPerson({x: 260, y: 180});
  stale.fields.name = "陈旧国名";
  expectUnchanged(store, () => store.commitEdit(stale), /过期/);
  assert.ok(!/<script[^>]+src=|<link[^>]+href="https?:/.test(html), "Preview must work offline");
  assert.ok(!html.includes("innerHTML"), "Dynamic labels must be built with textContent");
});

test("facts, frozen terrain, age and inventory cannot be overwritten; notes stay in view state", () => {
  const store = fresh(), refs = store.fixtureRefs();
  ["account", "reservation", "stock_batch", "contract", "garment", "history_event", "name_record", "genesis_node"].forEach(kind => {
    expectUnchanged(store, () => store.beginEdit(refs[kind]), /事实记录/);
    expectUnchanged(store, () => store.commitEdit({ref: refs[kind], expectedRevision: store.revisions().simRevision, fields: {name: "篡改"}}), /事实记录/);
    const before = worldSnapshot(store), draft = store.beginNote(refs[kind]);
    draft.text = "个人观察，不改变事实";
    store.commitNote(draft);
    assert.equal(worldSnapshot(store), before);
  });
  ["age", "naturalHeight", "worldScale", "balance", "stockUnits", "nationality"].forEach(field => {
    const draft = store.beginEdit(refs.person);
    draft.fields[field] = 0;
    expectUnchanged(store, () => store.commitEdit(draft), /字段不可编辑/);
  });
});

test("work/war/treaty display aliases are view state and cannot rewrite original facts", () => {
  const store = fresh(), refs = store.fixtureRefs(), before = worldSnapshot(store);
  ["innovation", "document", "war", "treaty"].forEach(kind => {
    const originalName = store.resolve(refs[kind]).name, draft = store.beginEdit(refs[kind]);
    assert.equal(draft.editScope, "view_alias");
    draft.fields.name = "我的观察别名 · " + kind;
    store.commitEdit(draft);
    assert.equal(store.resolve(refs[kind]).name, originalName);
    assert.equal(store.displayName(refs[kind]), draft.fields.name);
    assert.ok(store.search(draft.fields.name).some(ref => M.sameRef(ref, refs[kind])));
  });
  assert.equal(worldSnapshot(store), before);
});

test("classified clear previews explicit partial execution and protects residents, stores and access", () => {
  const store = fresh(), refs = store.fixtureRefs(), before = worldSnapshot(store);
  const preview = store.previewClear(["building", "road", "stock_batch"]);
  assert.equal(worldSnapshot(store), before, "Preview must not mutate the world");
  assert.ok(preview.candidates.some(c => M.sameRef(c.ref, refs.building) && !c.allowed));
  assert.ok(preview.candidates.some(c => M.sameRef(c.ref, refs.warehouse) && !c.allowed));
  assert.ok(preview.candidates.some(c => M.sameRef(c.ref, refs.road) && !c.allowed));
  assert.ok(preview.candidates.some(c => M.sameRef(c.ref, refs.stock_batch) && !c.allowed));
  const outcome = store.commitClear(preview.token);
  assert.equal(outcome.cleared.length, 2);
  assert.equal(outcome.blocked.length, 4);
  [refs.building, refs.warehouse, refs.road, refs.stock_batch].forEach(r => assert.equal(store.resolve(r).status, "active"));
  assert.equal(store.resolve(refs.empty).status, "archived");
  assert.equal(store.resolve(refs.path).status, "archived");
  assert.deepEqual(plain(store.inspectReferences()), []);
  expectUnchanged(store, () => store.commitClear(preview.token), /已提交/);
});

test("clear stale previews reject the whole requested batch", () => {
  const store = fresh(), preview = store.previewClear(["plant", "building"]), refs = store.fixtureRefs();
  store.setWorldLaw("aging", false, store.revisions().simRevision);
  expectUnchanged(store, () => store.commitClear(preview.token), /过期/);
  assert.equal(store.resolve(refs.plant).status, "active");
  assert.equal(store.resolve(refs.empty).status, "active");
});

test("clearing life creates stable death archives while retaining favorites and relationship targets", () => {
  const store = fresh(), refs = store.fixtureRefs();
  store.toggleFavorite(refs.person);
  store.commitClear(store.previewClear(["person"], [refs.person]).token);
  const person = store.resolve(refs.person);
  assert.equal(person.status, "deceased");
  assert.ok(person.deathSource.includes("clear_life"));
  assert.ok(person.history.some(h => store.resolve(h.ref).eventKind === "death"));
  assert.ok(store.isFavorite(refs.person));
  assert.equal(store.resolve(store.resolve(refs.household).relations.find(r => M.sameRef(r.ref, refs.person)).ref).status, "deceased");
  store.navigate(refs.person);
  assert.equal(store.statistics(refs.state).dead, 1);
  assert.equal(store.statistics(store.worldRef()).living, 3);
  assert.equal(store.previewClear(["person"], [refs.person]).candidates.length, 0);
  assert.deepEqual(plain(store.inspectReferences()), []);
});

test("closing a mineral source preserves mined batches and frozen terrain", () => {
  const store = fresh(), refs = store.fixtureRefs(), stock = plain(store.resolve(refs.stock_batch));
  const frozen = {height: store.snapshot().world.naturalHeight, scale: store.snapshot().world.worldScale};
  store.commitClear(store.previewClear(["mineral_deposit"]).token);
  assert.equal(store.resolve(refs.mineral_deposit).fields.剩余量, "0份 / 示例记录");
  assert.equal(store.resolve(refs.mineral_deposit).fields.矿源状态, "已停用");
  assert.deepEqual(plain(store.resolve(refs.stock_batch)), stock);
  assert.equal(store.snapshot().world.naturalHeight, frozen.height);
  assert.equal(store.snapshot().world.worldScale, frozen.scale);
});

test("tree-only and all-plant clear categories select real subsets without duplicate targets", () => {
  const store = fresh(), refs = store.fixtureRefs();
  const treeOnly = store.previewClear(["trees"]);
  assert.deepEqual(plain(treeOnly.candidates.map(c => c.ref.id)), [refs.plant.id]);
  const plants = store.previewClear(["plants"]);
  assert.equal(plants.candidates.length, 2);
  assert.ok(plants.candidates.some(c => M.sameRef(c.ref, refs.herb)));
  assert.equal(store.previewClear(["trees", "plants"]).candidates.length, 2);
  store.commitClear(treeOnly.token);
  assert.equal(store.resolve(refs.plant).status, "archived");
  assert.equal(store.resolve(refs.herb).status, "active");
});

test("asset metrics deduplicate physical records, keep units separate, and return unavailable as null", () => {
  const store = fresh(), refs = store.fixtureRefs(), before = worldSnapshot(store);
  const world = store.statistics(refs.world), enterprise = store.statistics(refs.enterprise), state = store.statistics(refs.state);
  assert.equal(world.garmentCount, 1);
  assert.equal(world.stockBatchCount, 1);
  assert.deepEqual(plain(world.materialQuantities), [{unit: "份棉布", quantity: 12}]);
  assert.equal(world.inventoryUnits, null, "World mixed quantities must not be coerced into one unit");
  assert.equal(enterprise.stockBatchCount, 1);
  assert.equal(enterprise.garmentCount, 0, "Employee-owned garments are not enterprise property");
  assert.equal(state.garmentCount, null);
  assert.equal(store.statistics(refs.plant).living, null);
  assert.equal(store.statistics(refs.account).inventoryUnits, null);
  assert.equal(store.statistics(refs.stock_batch).inventoryUnits, 12);
  assert.equal(state.populationScope, "legal_nationality");
  assert.equal(store.statistics(refs.city).populationScope, "resident_location");
  assert.equal(enterprise.populationScope, "enterprise_staff");
  assert.equal(worldSnapshot(store), before);
});

test("search and comparison compute actual linked members and remain read-only", () => {
  const store = fresh(), refs = store.fixtureRefs(), before = worldSnapshot(store);
  assert.ok(store.search("林").some(r => M.sameRef(r, refs.person)));
  assert.ok(store.search("语言").some(r => r.kind === "language"));
  const rows = store.compare([refs.person, refs.mother, refs.father, refs.elder]);
  assert.deepEqual(plain(rows.map(r => r.living)), [1, 1, 1, 1]);
  assert.deepEqual(plain(rows.map(r => r.simulationAge)), [18, 42, 44, 72]);
  assert.throws(() => store.compare([refs.state, refs.enterprise]), /同类型/);
  assert.equal(worldSnapshot(store), before);
  expectUnchanged(store, () => store.compare([{...plain(refs.state), world_id: "another-world"}]), /跨世界/);
});

test("clear guards use live Home, goods location and access references even when caches are zero", () => {
  const fixture = M.createFixture(), refs = fixture.fixtureRefs;
  fixture.objects[refs.building.id].occupied = 0;
  fixture.objects[refs.building.id].stockUnits = 0;
  fixture.objects[refs.warehouse.id].stockUnits = 0;
  fixture.objects[refs.road.id].protectedAccess = false;
  const store = M.createStore(fixture), before = worldSnapshot(store);
  const preview = store.previewClear(["buildings", "roads"], [refs.building, refs.warehouse, refs.road]);
  assert.equal(preview.candidates.length, 3);
  assert.ok(preview.candidates.every(c => !c.allowed));
  assert.ok(preview.candidates.find(c => M.sameRef(c.ref, refs.warehouse)).reason.includes("实际物品位置"));
  assert.ok(preview.candidates.find(c => M.sameRef(c.ref, refs.road)).reason.includes("实际接入"));
  const result = store.commitClear(preview.token);
  assert.equal(result.cleared.length, 0);
  assert.equal(worldSnapshot(store), before);
});

test("missing goods quantities stay unknown instead of fabricated zero totals", () => {
  const fixture = M.createFixture(), refs = fixture.fixtureRefs;
  delete fixture.objects[refs.garment.id].quantity;
  delete fixture.objects[refs.stock_batch.id].quantity;
  const store = M.createStore(fixture), stats = store.statistics(refs.world);
  assert.equal(stats.garmentCount, null);
  assert.equal(stats.garmentObjectCount, 1);
  assert.equal(stats.stockBatchCount, 1);
  assert.equal(stats.materialQuantities, null);
  assert.equal(store.statistics(refs.stock_batch).inventoryUnits, null);
});

test("player markers persist in a separate view snapshot and clearing them never changes sim revision", () => {
  const store = fresh(), refs = store.fixtureRefs(), before = worldSnapshot(store);
  const marker = store.createMarker({x: 220, y: 130}, "港口观察", refs.port);
  const draft = store.beginEdit(marker);
  draft.fields.name = "港口记事";
  store.commitEdit(draft);
  const preview = store.previewClear(["map_marker"], [marker]);
  store.commitClear(preview.token);
  assert.equal(worldSnapshot(store), before);
  assert.equal(store.resolve(marker).status, "archived");
  assert.ok(store.snapshot().view.markers[marker.id]);
  assert.ok(store.isFavorite(marker));
  assert.deepEqual(plain(store.inspectReferences()), []);
});



test("interface locales share all messages and are exact offline mirrors", () => {
  const registry = JSON.parse(fs.readFileSync(path.join(root,"data/ui_locales.json"),"utf8"));
  const embedded = JSON.parse(html.match(/<script id="world-inspector-locales" type="application\/json">([\s\S]*?)<\/script>/)[1]);
  assert.deepEqual(embedded, registry.messages);
  assert.deepEqual(Object.keys(registry.messages).sort(), ["de","en","zh"]);
  const keys = Object.keys(embedded.zh).sort();
  ["en","de"].forEach(locale => assert.deepEqual(Object.keys(embedded[locale]).sort(),keys));
  M.KINDS.forEach(kind=>assert.ok(embedded.en["kind."+kind] && embedded.de["kind."+kind]));
  M.LAW_IDS.forEach(id=>assert.ok(embedded.en["law."+id] && embedded.de["law."+id]));
  M.LAW_DEFINITIONS.forEach(d=>d.options.forEach(option=>assert.ok(embedded.en["option."+option] && embedded.de["option."+option])));
  assert.deepEqual(plain(M.FRONTEND_SECTIONS),["observe","people","civilization","construction","economy","world","settings"]);
});

test("UI locale changes preserve simulation language, object names and world draft revision", () => {
  const store=fresh(),refs=store.fixtureRefs(),before=worldSnapshot(store),draft=store.beginEdit(refs.person);
  ["en","de","zh"].forEach(locale=>{store.setLocale(locale);assert.equal(store.locale(),locale);assert.equal(worldSnapshot(store),before);assert.equal(store.resolve(refs.language).name,"青岬语");});
  expectUnchanged(store,()=>store.setLocale("fr"),/语言无效/);
  draft.fields.traits.curiosity=95;store.commitEdit(draft);assert.equal(store.resolve(refs.person).traits.curiosity,95);
});

test("six personality axes are bounded independent integers and cannot grant traits or property", () => {
  const store=fresh(),ref=store.fixtureRefs().person;
  const draft=store.beginEdit(ref);M.TRAIT_IDS.forEach(axis=>draft.fields.traits[axis]=100);store.commitEdit(draft);
  assert.deepEqual(plain(store.resolve(ref).traits),Object.fromEntries(plain(M.TRAIT_IDS).map(axis=>[axis,100])));
  [-1,101,1.5,"50"].forEach(value=>{const bad=store.beginEdit(ref);bad.fields.traits.curiosity=value;expectUnchanged(store,()=>store.commitEdit(bad),/0–100/);});
  const extra=store.beginEdit(ref);extra.fields.traits.immortal=1;expectUnchanged(store,()=>store.commitEdit(extra),/0–100/);
  const missing=store.beginEdit(ref);delete missing.fields.traits.piety;expectUnchanged(store,()=>store.commitEdit(missing),/0–100/);
});

test("state religion edits validate refs and preserve citizens, faith and church property", () => {
  const store=fresh(),refs=store.fixtureRefs(),person=plain(store.resolve(refs.person)),church=plain(store.resolve(refs.church));
  let draft=store.beginEdit(refs.state);draft.fields.religionBinding="state_faith";draft.fields.officialReligion=refs.religion;store.commitEdit(draft);
  assert.equal(store.resolve(refs.state).religionBinding,"state_faith");assert.ok(M.sameRef(store.resolve(refs.state).officialReligion,refs.religion));
  draft=store.beginEdit(refs.state);draft.fields.religionBinding="no_official";draft.fields.officialReligion=null;store.commitEdit(draft);
  assert.deepEqual(plain(store.resolve(refs.person)),person);assert.deepEqual(plain(store.resolve(refs.church)),church);
  [null,refs.person,{...plain(refs.religion),world_id:"foreign"}].forEach(religion=>{const bad=store.beginEdit(refs.state);bad.fields.religionBinding="state_faith";bad.fields.officialReligion=religion;expectUnchanged(store,()=>store.commitEdit(bad),/Religion|kind|跨世界/);});
  const sacred=store.beginEdit(refs.state);sacred.fields.religionBinding="sacred_crown";sacred.fields.officialReligion=refs.religion;expectUnchanged(store,()=>store.commitEdit(sacred),/资格/);
  const forged=store.beginEdit(refs.state);forged.fields.divineGrantRoot=refs.religion;expectUnchanged(store,()=>store.commitEdit(forged),/字段不可编辑/);
});

test("all 35 law schemas match and a proposal needs exact one-use Scheduler authorization", () => {
  const schema=JSON.parse(fs.readFileSync(path.join(root,"data/interaction_schema.json"),"utf8"));
  assert.deepEqual(plain(M.LAW_DEFINITIONS),schema.editing_contract.state_laws.definitions);
  const store=fresh(),refs=store.fixtureRefs(),draft=store.beginEdit(refs.state);
  draft.fields.laws.LAW_INCOME_TAX={option:"assessed",rateBasisPoints:1500,effectiveDay:90,transitionDays:90};
  expectUnchanged(store,()=>store.commitEdit(draft),/Scheduler批准/);
  expectUnchanged(store,()=>store.authorizeLawDraft(draft,"player"),/可信Scheduler/);
  const before=worldSnapshot(store);store.authorizeLawDraft(draft,"scheduler");assert.equal(worldSnapshot(store),before);
  draft.fields.laws.LAW_INCOME_TAX.rateBasisPoints=1600;
  expectUnchanged(store,()=>store.commitEdit(draft),/Scheduler批准/);
  store.authorizeLawDraft(draft,"scheduler");store.commitEdit(draft);
  assert.equal(store.resolve(refs.state).laws.LAW_INCOME_TAX.rateBasisPoints,1600);
  expectUnchanged(store,()=>store.commitEdit(draft),/过期/);
  assert.equal(store.resolve(refs.person).fields.食物单位,5);
});

test("invalid tax rates, options, dates and unapproved capability cannot partially change country religion", () => {
  const store=fresh(),refs=store.fixtureRefs();
  const cases=[{option:"invalid",rateBasisPoints:0,effectiveDay:30,transitionDays:30},{option:"assessed",rateBasisPoints:10001,effectiveDay:30,transitionDays:30},{option:"exempt",rateBasisPoints:1,effectiveDay:30,transitionDays:30},{option:"assessed",rateBasisPoints:500,effectiveDay:29,transitionDays:30},{option:"assessed",rateBasisPoints:500,effectiveDay:30,transitionDays:1}];
  cases.forEach(setting=>{const draft=store.beginEdit(refs.state);draft.fields.religionBinding="state_faith";draft.fields.officialReligion=refs.religion;draft.fields.laws.LAW_INCOME_TAX=setting;expectUnchanged(store,()=>store.authorizeLawDraft(draft,"scheduler"),/法条|税率/);expectUnchanged(store,()=>store.commitEdit(draft),/法条|税率/);});
  ["unlocked","constitution","treaty"].forEach(mode=>{const fixture=M.createFixture(),r=fixture.fixtureRefs,state=fixture.objects[r.state.id];if(mode==="unlocked")state.unlockedLawIds=[];if(mode==="constitution")state.constitutionalApproval=false;if(mode==="treaty")state.reservedLawIds=["LAW_INCOME_TAX"];const s=M.createStore(fixture),draft=s.beginEdit(r.state);draft.fields.laws.LAW_INCOME_TAX={option:"assessed",rateBasisPoints:500,effectiveDay:30,transitionDays:30};expectUnchanged(s,()=>s.authorizeLawDraft(draft,"scheduler"),/能力|批准|条约/);});
});

test("imperial and sacred roots cannot be removed through ordinary edits", () => {
  const fixture=M.createFixture(),refs=fixture.fixtureRefs,state=fixture.objects[refs.state.id];state.religionBinding="sacred_crown";state.officialReligion=refs.religion;state.divineGrantRoot=refs.religion;state.imperial=true;
  const store=M.createStore(fixture),draft=store.beginEdit(refs.state);draft.fields.religionBinding="no_official";draft.fields.officialReligion=null;
  expectUnchanged(store,()=>store.commitEdit(draft),/合法程序/);
});

// A small DOM adapter executes the shipped UI script itself, rather than a duplicate UI model.
class TestElement {
  constructor(tag,document){this.tagName=tag.toUpperCase();this.document=document;this.children=[];this.attributes={};this.listeners={};this.style={};this.hidden=false;this.disabled=false;this._value=undefined;this.textContent="";this.isConnected=true;}
  append(...children){children.forEach(child=>{this.children.push(child);child.parentElement=this;});}
  replaceChildren(...children){this.children=[];this.append(...children);}
  setAttribute(key,value){this.attributes[key]=String(value);}
  getAttribute(key){return this.attributes[key];}
  addEventListener(type,listener){(this.listeners[type]||(this.listeners[type]=[])).push(listener);}
  fire(type){(this.listeners[type]||[]).slice().forEach(listener=>listener({target:this,key:null,preventDefault(){},stopPropagation(){}}));}
  focus(){this.document.activeElement=this;}
  getClientRects(){return [{}];}
  querySelectorAll(selector){const tags=selector.split(",").map(x=>x.trim().toUpperCase());const found=[];const walk=node=>node.children.forEach(child=>{if(tags.includes(child.tagName)||(selector.includes("tabindex")&&child.attributes.tabindex==="0"))found.push(child);walk(child);});walk(this);return found;}
  set value(v){this._value=String(v);}
  get value(){if(this._value!==undefined)return this._value;if(this.tagName==="SELECT"){const selected=this.children.find(x=>x.selected)||this.children[0];return selected?selected.value:"";}return "";}
}
function uiFixture(){
  const document={activeElement:null,listeners:{},documentElement:{lang:""},addEventListener(type,listener){(this.listeners[type]||(this.listeners[type]=[])).push(listener);},createElement(tag){return new TestElement(tag,this);},createElementNS(ns,tag){return new TestElement(tag,this);}};
  const roots={};["world-inspector-locales","app-title","prototype-label","app-hint","disclaimer","status-bar","toolbar-sections","toolbar-tools","dock-hint","world-map","map-mode-label","map-hint","inspector","object-content","object-title","object-tabs","nav-back","nav-forward","inspector-close","nav-position","favorite-current","dialog-title","modal-close","modal","dialog-content","toast"].forEach(id=>{roots[id]=new TestElement("div",document);roots[id].id=id;});
  roots["modal"].hidden=true;roots["world-inspector-locales"].textContent=html.match(/<script id="world-inspector-locales" type="application\/json">([\s\S]*?)<\/script>/)[1];
  document.getElementById=id=>{if(roots[id])return roots[id];let result=null;const walk=node=>{if(node.id===id)result=node;node.children.forEach(walk);};Object.values(roots).forEach(walk);return result;};
  const ctx=vm.createContext({document,setTimeout(){return 1;},clearTimeout(){}});vm.runInContext(extract("world-inspector-model"),ctx);vm.runInContext(extract("world-inspector-ui"),ctx);return{ctx,document,roots,P:ctx.SonnheideInspectorPreview};
}

test("actual UI keeps global tools in bottom groups and cancels placement when switching section", () => {
  const {document,roots}=uiFixture();document.getElementById("section-people").fire("click");document.getElementById("tool-spawn").fire("click");assert.ok(document.getElementById("tool-cancel"));
  document.getElementById("section-world").fire("click");assert.equal(document.getElementById("tool-cancel"),null);assert.ok(document.getElementById("tool-clear"));assert.equal(roots.modal.hidden,true,"Section must not automatically arm clear");
  const footer=html.match(/<footer class="tool-dock"[\s\S]*?<\/footer>/)[0];assert.ok(footer.includes("toolbar-sections")&&footer.includes("toolbar-tools"));
  assert.ok(!html.includes("top-actions")&&!html.includes('class="panel directory"'));
});

test("actual UI preserves dirty form through section and locale changes, then guards close", () => {
  const {document,roots,P}=uiFixture(),before=worldSnapshot(P.store);P.openObject(P.store.fixtureRefs().person);
  const edit=roots["object-tabs"].children.find(button=>button.textContent==="编辑");edit.fire("click");let name=document.getElementById("edit-name");name.value="保留草稿";name.fire("input");
  document.getElementById("section-settings").fire("click");assert.equal(document.getElementById("edit-name").value,"保留草稿");document.getElementById("tool-locale").fire("click");document.getElementById("locale-en").fire("click");assert.equal(document.getElementById("edit-name").value,"保留草稿");assert.equal(document.documentElement.lang,"en");
  document.getElementById("locale-de").fire("click");assert.equal(document.getElementById("edit-name").value,"保留草稿");assert.equal(document.documentElement.lang,"de");assert.equal(worldSnapshot(P.store),before);
  document.getElementById("modal-close").fire("click");document.getElementById("inspector-close").fire("click");assert.equal(roots["dialog-title"].textContent,"Es gibt einen nicht angewandten Entwurf");assert.equal(roots.inspector.hidden,false);
});



test("pre-cultural capability alone cannot enable private property or religious institutions", () => {
  const fixture=M.createFixture(),refs=fixture.fixtureRefs,state=fixture.objects[refs.state.id];state.cultural=false;state.laws={};
  const store=M.createStore(fixture),draft=store.beginEdit(refs.state);draft.fields.laws.LAW_PROPERTY_REGIME={option:"public",rateBasisPoints:null,effectiveDay:30,transitionDays:30};store.authorizeLawDraft(draft,"scheduler");store.commitEdit(draft);
  const bad=store.beginEdit(refs.state);bad.fields.laws.LAW_PROPERTY_REGIME.option="private_open";expectUnchanged(store,()=>store.authorizeLawDraft(bad,"scheduler"),/前文化/);
  const religion=store.beginEdit(refs.state);religion.fields.religionBinding="state_faith";religion.fields.officialReligion=refs.religion;expectUnchanged(store,()=>store.commitEdit(religion),/文化/);
});

test("religion changes supersede old official-policy summaries and require dependency transitions", () => {
  const fixture=M.createFixture(),refs=fixture.fixtureRefs,state=fixture.objects[refs.state.id];state.religionBinding="state_faith";state.officialReligion=refs.religion;state.laws.LAW_OFFICIAL_RELIGION={option:"state_faith",rateBasisPoints:null,effectiveDay:30,transitionDays:30};
  const store=M.createStore(fixture),draft=store.beginEdit(refs.state);draft.fields.religionBinding="official_tolerant";store.commitEdit(draft);const after=store.resolve(refs.state);assert.equal(after.laws.LAW_OFFICIAL_RELIGION,undefined);assert.equal(after.supersededLaws[0].option,"state_faith");assert.ok(after.history.some(h=>h.source==="state:policy_superseded"));
  const f=M.createFixture(),r=f.fixtureRefs,s=f.objects[r.state.id];s.religionBinding="state_faith";s.officialReligion=r.religion;s.laws.LAW_CHURCH_BUSINESS={option:"charter_intersection",rateBasisPoints:null,effectiveDay:30,transitionDays:30};const st=M.createStore(f),unbinding=st.beginEdit(r.state);unbinding.fields.religionBinding="no_official";unbinding.fields.officialReligion=null;expectUnchanged(st,()=>st.commitEdit(unbinding),/停用|过渡/);
});

test("actual German directory search uses localized type labels", () => {
  const {document,roots,P}=uiFixture();P.store.setLocale("de");document.getElementById("tool-search").fire("click");const search=document.getElementById("search");search.value="Sprache";search.fire("input");const buttons=roots["dialog-content"].querySelectorAll("button");assert.ok(buttons.some(b=>b.textContent.includes("青岬语")));
});


test("actual UI preserves unsaved readonly-object notes through locale changes and guards navigation", () => {
  const {document,roots,P}=uiFixture();P.openObject(P.store.fixtureRefs().account);
  roots["object-tabs"].children.find(button=>button.textContent==="编辑").fire("click");const note=document.getElementById("edit-note");note.value="等待核对的未保存备注";note.fire("input");document.getElementById("section-settings").fire("click");document.getElementById("tool-locale").fire("click");document.getElementById("locale-en").fire("click");assert.equal(document.getElementById("edit-note").value,note.value);document.getElementById("modal-close").fire("click");P.openObject(P.store.fixtureRefs().state);assert.equal(roots["dialog-title"].textContent,"You have an unapplied draft");assert.equal(P.store.current().kind,"account");
});


test("actual preview lists only actual changes, with explicit before and after values", () => {
  const {P,document,roots}=uiFixture(),ref=P.store.fixtureRefs().person;P.openObject(ref);
  roots["object-tabs"].children.find(button=>button.textContent==="编辑").fire("click");
  const draft=P.store.beginEdit(ref);draft.originalFields=JSON.stringify(draft.fields);assert.deepEqual(plain(P.draftChanges(draft)),[]);
  draft.fields.traits.curiosity=95;assert.deepEqual(plain(P.draftChanges(draft)),[{label:"求知",before:70,after:95}]);
  const preview=roots["object-content"].querySelectorAll("button").find(b=>b.textContent==="预览变更");preview.fire("click");assert.equal(roots["dialog-content"].children[0].textContent,"草稿没有实际变化，无需提交。");assert.ok(roots["dialog-content"].querySelectorAll("button").find(b=>b.textContent==="提交草稿").disabled);
  document.getElementById("modal-close").fire("click");const axis=document.getElementById("trait-curiosity");axis.value="95";axis.fire("input");preview.fire("click");assert.equal(roots["dialog-content"].children[0].textContent,"实际变更1项；确认后一起提交。");
  const change=roots["dialog-content"].children.find(child=>child.attributes["data-change"]==="true");assert.equal(change.children[0].textContent,"求知");assert.equal(change.children[1].children[0].textContent,"70");assert.equal(change.children[1].children[2].textContent,"95");
});

test("new, changed and removed laws preview all parameters, including unchanged law-count tax edits", () => {
  const {P}=uiFixture(),ref=P.store.fixtureRefs().state;P.store.setLocale("en");
  const draft=P.store.beginEdit(ref);draft.originalFields=JSON.stringify(draft.fields);draft.fields.laws.LAW_INCOME_TAX={option:"assessed",rateBasisPoints:1250,effectiveDay:90,transitionDays:90};
  let changes=plain(P.draftChanges(draft));assert.equal(changes.length,4);assert.ok(changes.every(row=>row.label.startsWith("Personal Income Tax · ")));assert.deepEqual(changes.map(row=>row.after),["Assessed","1250","90","90"]);assert.ok(changes.every(row=>row.before==="Not recorded"));
  draft.originalFields=JSON.stringify(draft.fields);draft.fields.laws.LAW_INCOME_TAX.rateBasisPoints=1500;changes=plain(P.draftChanges(draft));assert.deepEqual(changes,[{label:"Personal Income Tax · Rate / share (basis points, 100=1%)",before:"1250",after:"1500"}]);
  draft.originalFields=JSON.stringify(draft.fields);delete draft.fields.laws.LAW_INCOME_TAX;changes=plain(P.draftChanges(draft));assert.equal(changes.length,4);assert.ok(changes.every(row=>row.after==="Not recorded"));assert.ok(changes.some(row=>row.before==="1500"));
});

test("state edit controls have explicit translated accessible labels and war status stays outside its name", () => {
  const {P,document,roots}=uiFixture();P.store.setLocale("de");P.openObject(P.store.fixtureRefs().state);roots["object-tabs"].children.find(button=>button.textContent==="Bearbeiten").fire("click");
  assert.equal(document.getElementById("edit-religion-binding").attributes["aria-label"],"Staat-Religion-Regelung");assert.equal(document.getElementById("edit-official-religion").attributes["aria-label"],"Offizielle Religion");assert.equal(document.getElementById("law-rate-LAW_INCOME_TAX").attributes["aria-label"],"Einkommensteuer · Satz / Anteil (Basispunkte, 100=1 %)");
  assert.equal(P.store.resolve(P.store.fixtureRefs().war).name,"北岸战争");
});


test("actual UI resets scroll for new objects and tabs but preserves same-page locale and section scroll", () => {
  const {P,document,roots}=uiFixture();P.openObject(P.store.fixtureRefs().state);
  roots["object-tabs"].children.find(button=>button.textContent==="编辑").fire("click");roots["object-content"].scrollTop=340;
  document.getElementById("section-settings").fire("click");assert.equal(roots["object-content"].scrollTop,340);
  document.getElementById("tool-locale").fire("click");document.getElementById("locale-en").fire("click");assert.equal(roots["object-content"].scrollTop,340,"Locale re-render must preserve same-object same-tab scroll");document.getElementById("modal-close").fire("click");
  roots["object-tabs"].children.find(button=>button.textContent==="Relations").fire("click");assert.equal(roots["object-content"].scrollTop,0,"New tab must start at its first field");roots["object-content"].scrollTop=180;
  P.openObject(P.store.fixtureRefs().person);assert.equal(roots["object-content"].scrollTop,0,"New object must not inherit the previous object's scroll");roots["object-tabs"].children.find(button=>button.textContent==="Edit").fire("click");assert.equal(roots["object-content"].scrollTop,0);assert.ok(document.getElementById("edit-name"));
  roots["object-content"].scrollTop=120;document.getElementById("tool-locale").fire("click");document.getElementById("locale-de").fire("click");assert.equal(roots["object-content"].scrollTop,120);
});
process.stdout.write("Validated " + count + " total model and shipped-UI interaction scenarios.\n");
