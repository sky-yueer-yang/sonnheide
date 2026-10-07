#!/usr/bin/env node
"use strict";
// Extra presentation checks. The existing 39 interaction tests remain independent and unchanged.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const root = path.resolve(__dirname, "..");
const html = fs.readFileSync(path.join(root, "tools/previews/world-inspector.html"), "utf8");
const script = id => {
  const match = html.match(new RegExp('<script id="' + id + '">([\\s\\S]*?)<\\/script>'));
  assert.ok(match, "Missing shipped script " + id);
  return match[1];
};
let count = 0;
function test(label, run) { run(); count++; process.stdout.write("PASS " + label + "\n"); }

class Element {
  constructor(tag, document) {
    this.tagName = tag.toUpperCase(); this.document = document;
    this.children = []; this.attributes = {}; this.listeners = {}; this.style = {};
    this.hidden = false; this.disabled = false; this.isConnected = true; this._text = "";
  }
  set textContent(value) { this._text = String(value); this.children = []; }
  get textContent() { return this._text + this.children.map(child => child.textContent).join(""); }
  append(...children) { children.forEach(child => { this.children.push(child); child.parentElement = this; }); }
  replaceChildren(...children) { this._text = ""; this.children = []; this.append(...children); }
  setAttribute(key, value) { this.attributes[key] = String(value); }
  getAttribute(key) { return this.attributes[key]; }
  addEventListener(type, listener) { (this.listeners[type] ||= []).push(listener); }
  fire(type, extra = {}) { (this.listeners[type] || []).slice().forEach(fn => fn({target: this, preventDefault() {}, stopPropagation() {}, ...extra})); }
  focus() { this.document.activeElement = this; }
  scrollIntoView() {}
  getClientRects() { return this.visible() ? [{}] : []; }
  visible() { for (let node = this; node; node = node.parentElement) if (node.hidden) return false; return true; }
  querySelectorAll(selector) {
    const tags = selector.split(",").map(s => s.trim().toUpperCase());
    return descendants(this).filter(child => tags.includes(child.tagName) || (selector.includes("tabindex") && child.attributes.tabindex === "0"));
  }
  set value(value) { this._value = String(value); }
  get value() {
    if (this._value !== undefined) return this._value;
    if (this.tagName === "SELECT") return (this.children.find(o => o.selected) || this.children[0])?.value || "";
    return "";
  }
}
function descendants(root) { const nodes = []; const visit = node => node.children.forEach(child => { nodes.push(child); visit(child); }); visit(root); return nodes; }
function fixture() {
  const document = {activeElement: null, hidden: false, listeners: {}, documentElement: {lang: ""},
    addEventListener(type, listener) { (this.listeners[type] ||= []).push(listener); },
    createElement(tag) { return new Element(tag, this); }, createElementNS(_ns, tag) { return new Element(tag, this); }};
  document.body = new Element("body", document);
  const roots = {};
  for (const match of html.matchAll(/<([a-z][\w-]*)\b[^>]*\bid="([^"]+)"[^>]*>/gi)) {
    roots[match[2]] = new Element(match[1], document); roots[match[2]].id = match[2];
    if (/\bhidden\b/.test(match[0])) roots[match[2]].hidden = true;
  }
  roots["world-inspector-locales"].textContent = html.match(/<script id="world-inspector-locales" type="application\/json">([\s\S]*?)<\/script>/)[1];
  roots.inspector.append(...["nav-back", "nav-forward", "favorite-current", "inspector-close", "object-title", "object-tabs", "object-content"].map(id => roots[id]));
  roots.modal.append(roots["dialog-title"], roots["modal-close"], roots["dialog-content"]);
  roots["tool-dock"].append(roots["toolbar-sections"], roots["toolbar-tools"]);
  Object.values(roots).filter(node => !node.parentElement).forEach(node => document.body.append(node));
  document.getElementById = id => descendants(document.body).find(node => node.id === id) || null;
  let timerId = 0; const intervals = new Map(), timeouts = new Map();
  const context = vm.createContext({document,
    setTimeout(fn, ms) { const id = ++timerId; timeouts.set(id, {fn, ms}); return id; },
    clearTimeout(id) { timeouts.delete(id); },
    setInterval(fn, ms) { const id = ++timerId; intervals.set(id, {fn, ms}); return id; },
    clearInterval(id) { intervals.delete(id); }});
  vm.runInContext(script("world-inspector-model"), context);
  vm.runInContext(script("world-inspector-ui"), context);
  return {document, roots, context, P: context.SonnheideInspectorPreview, intervals, timeouts};
}
function assertPurposefulControls(f) {
  const buttons = descendants(f.document.body).filter(node => node.tagName === "BUTTON" && node.visible());
  assert.ok(buttons.length);
  buttons.forEach(button => {
    const hasIcon = button.children.some(child => child.tagName === "SVG");
    const toolbar = /^(section-|tool-)/.test(button.id || "");
    const presentation = /^presentation-/.test(button.id || "");
    const iconOnly = !button.textContent.trim();
    if (toolbar || iconOnly) assert.ok(hasIcon, "Functional recognition icon missing: " + (button.id || button.textContent));
    if (hasIcon) assert.ok(toolbar || presentation || iconOnly, "An ordinary text action has an unnecessary decoration: " + button.textContent);
    assert.ok(button.getAttribute("aria-label"), "Missing accessible name: " + button.textContent);
    assert.ok(button.title, "Missing invoked tooltip: " + button.textContent);
    assert.ok(!/[\u{1F300}-\u{1FAFF}]/u.test(button.textContent), "Emoji must not substitute for functional SVG icons");
  });
}
function clickText(f, root, text) { const b = root.querySelectorAll("button").find(node => node.textContent === text); assert.ok(b, "Missing button " + text); b.fire("click"); }

test("every button style is frameless and keyboard focus has a visible solid treatment", () => {
  const css = html.match(/<style>([\s\S]*?)<\/style>/)[1];
  for (const match of css.matchAll(/([^{}]+)\{([^{}]*)\}/g)) {
    if (!/\bbutton\b/.test(match[1])) continue;
    for (const property of match[2].matchAll(/(?:^|;)\s*(border(?:-(?:top|right|bottom|left|color|width|style))?|outline|box-shadow)\s*:\s*([^;]+)/g)) {
      assert.ok(/^(0|none)$/.test(property[2].trim()), "Button frame style: " + property[0]);
    }
  }
  assert.ok(/button:focus-visible[^{]*\{[^}]*background:#4b4030[^}]*color:#fff5df/.test(css));
  assert.ok(css.includes("--panel:#1c1e20") && css.includes("--ink:#e7dfcf"));
  assert.ok(!/<script[^>]+src=|<link[^>]+href="https?:/.test(html));
});

test("all seven dock sections are original SVG icons with translated labels and hold hints", () => {
  const f = fixture();
  for (const locale of ["zh", "en", "de"]) {
    f.P.store.setLocale(locale); f.P.openObject(f.P.store.fixtureRefs().person);
    assert.equal(f.roots["toolbar-sections"].children.length, 7);
    f.roots["toolbar-sections"].children.forEach(button => {
      assert.ok(button.children.some(node => node.tagName === "SVG"));
      assert.ok(button.listeners.pointerdown?.length, "Missing long-press hint");
      assert.ok(button.listeners.focus?.length, "Missing focus hint");
      button.fire("click"); assertPurposefulControls(f);
    });
  }
});

test("all 40 pages retain named actions and six clear text tabs without decorative icons", () => {
  const f = fixture(), model = f.context.SonnheideInspectorModel;
  const initialWorld = JSON.stringify(f.P.store.snapshot().world);
  for (const locale of ["zh", "en", "de"]) {
    f.P.store.setLocale(locale);
    for (const kind of model.KINDS) {
      const object = f.P.store.all().find(o => o.kind === kind);
      for (const tab of model.INSPECTOR_SECTIONS) {
        f.P.openObject(f.P.store.ref(object), tab); assertPurposefulControls(f);
        assert.equal(f.roots["object-tabs"].children.length, 6);
        f.roots["object-tabs"].children.forEach(button => {
          assert.ok(button.textContent.trim());
          assert.ok(!button.children.some(node => node.tagName === "SVG"), "Readable archive tabs do not need a repeated glyph");
        });
      }
    }
  }
  assert.equal(JSON.stringify(f.P.store.snapshot().world), initialWorld);
});

test("dynamic dialogs retain purposeful controls and readable text actions", () => {
  const f = fixture();
  const dialogs = {observe: ["search", "favorites", "compare"], people: ["archives"], world: ["laws", "environment", "clear"], settings: ["locale", "help", "reset"]};
  for (const [section, tools] of Object.entries(dialogs)) {
    f.document.getElementById("section-" + section).fire("click");
    for (const tool of tools) {
      f.document.getElementById("tool-" + tool).fire("click"); assertPurposefulControls(f);
      if (tool === "clear") {
        const checkbox = f.roots["dialog-content"].querySelectorAll("input")[0];
        checkbox.checked = true; checkbox.fire("change");
        clickText(f, f.roots["dialog-content"], f.P.translate("action.previewClear")); assertPurposefulControls(f);
      }
      f.document.getElementById("modal-close").fire("click");
    }
  }
  f.P.openObject(f.P.store.fixtureRefs().person, "edit");
  clickText(f, f.roots["object-content"], f.P.translate("action.preview")); assertPurposefulControls(f);
  f.document.getElementById("modal-close").fire("click");
  const name = f.document.getElementById("edit-name"); name.value = "未提交名称"; name.fire("input");
  f.P.openObject(f.P.store.fixtureRefs().state); assertPurposefulControls(f);
  assert.equal(f.roots["dialog-title"].textContent, f.P.translate("edit.dirtyTitle"));
});

test("environment presentation starts with automatic demonstration explicitly disabled", () => {
  const f = fixture();
  assert.equal(f.P.presentation.snapshot().age, "light");
  assert.equal(f.P.presentation.snapshot().automatic, false);
  assert.equal(f.intervals.size, 0);
  assert.equal(f.document.body.getAttribute("data-age"), "light");
  assert.equal(f.roots["age-display"].textContent, "光明纪元");
  assert.equal(f.roots["age-display"].children.length, 1);
  f.document.getElementById("section-world").fire("click");
  f.document.getElementById("tool-environment").fire("click");
  assert.ok(f.roots["dialog-content"].textContent.includes("不改变世界规则"), "Truthful presentation scope belongs in the invoked environment panel");
  f.P.presentation.tick();
  assert.equal(f.P.presentation.snapshot().age, "light", "No implicit environment cycle");
});

test("manual light/dark controls and explicit auto ticks never change world, view, locale or statistics", () => {
  const f = fixture();
  f.document.getElementById("section-world").fire("click");
  f.document.getElementById("tool-environment").fire("click");
  const before = JSON.stringify(f.P.store.snapshot()), statistics = JSON.stringify(f.P.store.statistics(f.P.store.worldRef()));
  f.document.getElementById("presentation-darkness").fire("click");
  assert.equal(f.P.presentation.snapshot().age, "darkness");
  f.document.getElementById("presentation-auto").fire("click");
  assert.equal(f.P.presentation.snapshot().automatic, true);
  assert.ok(f.roots["age-display"].textContent.includes("示例轮替"), "An explicitly running automatic example remains identified");
  assert.equal(f.intervals.size, 1);
  assert.equal([...f.intervals.values()][0].ms, 18000);
  [...f.intervals.values()][0].fn();
  assert.equal(f.P.presentation.snapshot().age, "light");
  f.document.hidden = true; [...f.intervals.values()][0].fn();
  assert.equal(f.P.presentation.snapshot().age, "light", "Background tabs do not advance presentation");
  f.document.hidden = false;
  f.document.getElementById("presentation-darkness").fire("click");
  assert.equal(f.P.presentation.snapshot().automatic, false);
  assert.equal(f.intervals.size, 0);
  assert.equal(JSON.stringify(f.P.store.snapshot()), before);
  assert.equal(JSON.stringify(f.P.store.statistics(f.P.store.worldRef())), statistics);
});

test("age labels localize separately from the frozen message catalog", () => {
  const f = fixture(), labels = f.context.SonnheidePresentationModel.labels;
  for (const locale of ["zh", "en", "de"]) {
    f.P.store.setLocale(locale); f.P.openObject(f.P.store.fixtureRefs().person);
    f.document.getElementById("section-world").fire("click");
    const entry = f.document.getElementById("tool-environment");
    assert.equal(entry.getAttribute("aria-label"), labels[locale].environment);
    entry.fire("click");
    assert.equal(f.roots["dialog-title"].textContent, labels[locale].environment);
    f.document.getElementById("presentation-darkness").fire("click");
    assert.ok(f.roots["age-display"].textContent.includes(labels[locale].darkness));
    assert.equal(f.document.getElementById("presentation-darkness").textContent, labels[locale].darkness);
    if (locale === "zh") {
      assert.equal(labels[locale].light, "光明纪元");
      assert.equal(labels[locale].darkness, "黑暗纪元");
      assert.ok(!JSON.stringify(labels[locale]).includes("时代"));
    }
    assert.ok(f.document.getElementById("presentation-auto").getAttribute("aria-label"));
    assert.equal(f.P.store.locale(), locale);
  }
});

test("global controls stay in the bottom dock and environment parameters exist only in their world-section panel", () => {
  const f = fixture();
  const inTree = (node, root) => node === root || descendants(root).includes(node);
  const assertGlobalPlacement = () => {
    assert.equal(f.roots["age-display"].querySelectorAll("button,input,select").length, 0, "The upper status display must remain read-only");
    descendants(f.document.body).filter(node => node.tagName === "BUTTON" && /^(tool-|section-)/.test(node.id || "")).forEach(node => {
      assert.ok(inTree(node, f.roots["tool-dock"]), "Global tool outside bottom dock: " + node.id);
    });
  };
  assertGlobalPlacement();
  for (const section of Object.keys(f.P.groups)) {
    f.document.getElementById("section-" + section).fire("click");
    assertGlobalPlacement();
    ["light", "darkness", "auto"].forEach(id => {
      const control = f.document.getElementById("presentation-" + id);
      assert.ok(!control || !control.visible(), "Environment parameters must not stay visible after their panel closes");
    });
    const entry = f.document.getElementById("tool-environment");
    if (section !== "world") { assert.equal(entry, null); continue; }
    assert.ok(entry && inTree(entry, f.roots["toolbar-tools"]));
    const lightGlyph = entry.children.find(node => node.tagName === "SVG").children[0].getAttribute("d");
    entry.fire("click");
    ["light", "darkness", "auto"].forEach(id => {
      const control = f.document.getElementById("presentation-" + id);
      assert.ok(control && control.visible() && inTree(control, f.roots["dialog-content"]));
    });
    assertGlobalPlacement(); assertPurposefulControls(f);
    f.document.getElementById("presentation-darkness").fire("click");
    const moonGlyph = f.document.getElementById("tool-environment").children.find(node => node.tagName === "SVG").children[0].getAttribute("d");
    assert.notEqual(moonGlyph, lightGlyph, "Bottom environment glyph tracks current light/moon presentation");
    f.document.getElementById("modal-close").fire("click");
    assert.equal(f.roots.modal.hidden, true);
    ["light", "darkness", "auto"].forEach(id => assert.equal(f.document.getElementById("presentation-" + id).visible(), false));
  }
});

test("quiet default chrome has no tiny debug copy while invoked help preserves the truthful scope", () => {
  const f = fixture();
  const removed = ["app-title", "prototype-label", "app-hint", "status-bar", "disclaimer", "map-mode-label", "map-hint", "dock-hint", "nav-position"];
  removed.forEach(id => assert.equal(f.document.getElementById(id), null, "Obsolete constant chrome: " + id));
  assert.equal(f.roots["age-display"].textContent, "光明纪元");
  assert.ok(f.roots["control-hint"].hidden, "Hints are invoked, not permanently overlaid");
  const css = html.match(/<style>([\s\S]*?)<\/style>/)[1];
  for (const match of css.matchAll(/font-size\s*:\s*([0-9.]+)px/g)) {
    assert.ok(Number(match[1]) >= 12, "Interface copy is squeezed into unnecessary tiny text: " + match[0]);
  }
  for (const locale of ["zh", "en", "de"]) {
    f.P.store.setLocale(locale);
    f.P.openObject(f.P.store.fixtureRefs().state, "statistics");
    assert.ok(!f.roots["object-content"].textContent.includes(f.P.translate("page.coverageValue")), "No revision/source-count pseudo-debug in normal statistics");
    assert.ok(!f.roots["object-title"].textContent.includes(f.P.translate("page.stableRef")), "Stable protocol IDs do not crowd the archive title");
    const before = JSON.stringify(f.P.store.snapshot());
    f.document.getElementById("section-settings").fire("click");
    f.document.getElementById("tool-help").fire("click");
    for (const key of ["app.prototype", "app.disclaimer", "help.population", "page.statisticsNote"]) {
      assert.ok(f.roots["dialog-content"].textContent.includes(f.P.translate(key)), "Invoked help lost honest scope: " + key);
    }
    assert.equal(JSON.stringify(f.P.store.snapshot()), before, "Opening truthful help does not alter world or player state");
    assertPurposefulControls(f);
    f.document.getElementById("modal-close").fire("click");
  }
});

process.stdout.write("Validated " + count + " additional shipped-UI presentation scenarios.\n");
