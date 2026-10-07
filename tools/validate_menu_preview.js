#!/usr/bin/env node
"use strict";

// Executes the real menu script. The small DOM/image/RAF adapter supplies browser primitives,
// including independent preload success, visible-layer failure and deferred decode completion.
// It does not reimplement painting selection, transitions, timing or locale decisions.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const source = fs.readFileSync(path.join(__dirname, "previews/main-menu.js"), "utf8");
const html = fs.readFileSync(path.join(__dirname, "previews/main-menu.html"), "utf8");
const css = fs.readFileSync(path.join(__dirname, "previews/main-menu.css"), "utf8");

class Element {
  constructor(id="", tagName="button") {
    this.id=id;this.tagName=tagName;this.dataset={};this.style={};this.attributes={};this.listeners={};this.children=[];
    const classes=new Set();this.classList={add:v=>classes.add(v),remove:v=>classes.delete(v),contains:v=>classes.has(v),toggle:(v,on)=>on?classes.add(v):classes.delete(v)};
    this.complete=true;this.naturalWidth=1536;this.decode=()=>Promise.resolve();this._src="";
  }
  addEventListener(type,listener){(this.listeners[type] ||= []).push(listener);}
  emit(type,event={}){for(const listener of this.listeners[type] || [])listener({target:this,...event});}
  setAttribute(key,value){this.attributes[key]=value;}
  querySelector(selector){if(selector==="use")return this.use ||= new Element("","use");return null;}
  querySelectorAll(){return [];}
  matches(selector){return selector===this.tagName;}
  closest(selector){return this.matches(selector)?this:null;}
  appendChild(child){this.children.push(child);return child;}
  replaceChildren(...children){this.children=children;}
  focus(){}
  showModal(){this.open=true;}
  close(){this.open=false;this.emit("close");}
  getBoundingClientRect(){return {left:0,right:680,top:0,bottom:500};}
  set src(value){this._src=value;this.complete=false;this.naturalWidth=0;}
  get src(){return this._src;}
}

function harness({reduced=false}={}) {
  const elements=new Map();const get=id=>{if(!elements.has(id))elements.set(id,new Element(id));return elements.get(id);};
  const actions=["new","continue","load","settings","quit"].map(action=>{const element=new Element();element.dataset.action=action;element.disabled=action==="continue";return element;});
  const document=new Element("document","document");document.hidden=false;document.body=new Element("body","body");document.documentElement=new Element("html","html");
  document.getElementById=get;document.querySelectorAll=selector=>selector===".main-nav [data-action]"?actions:[];
  document.createElement=tag=>new Element("",tag);document.createTextNode=text=>({textContent:text});
  get("settings-template").content={cloneNode:()=>new Element("settings-clone","div")};
  const images=[];class Preload {constructor(){images.push(this);}}
  const preferences=new Map();const localStorage={getItem:key=>preferences.get(key)||null,setItem:(key,value)=>preferences.set(key,value)};
  const media={matches:reduced,addEventListener(){}};const window={matchMedia:()=>media};
  let nextFrame=null;let timestamp=1;
  const context={document,window,location:{search:""},localStorage,Image:Preload,URLSearchParams,requestAnimationFrame:fn=>{nextFrame=fn;}};
  for(const forbidden of ["World","world","simulation","nativeBridge","fetch","XMLHttpRequest"])
    Object.defineProperty(context,forbidden,{get(){throw new Error(`Unexpected world/network access: ${forbidden}`);}});
  vm.runInNewContext(source,context,{filename:"main-menu.js"});
  nextFrame(timestamp);
  return {
    get,document,actions,images,
    state:()=>JSON.parse(JSON.stringify(window.SonnheideMenuPreview.getState())),
    preloads(ok=true){for(const image of images){assert.match(image.src,/^\.\.\/\.\.\/assets\/source\/ui\/paintings\/painting-0[1-5]\.jpg$/);image[ok?"onload":"onerror"]();}},
    click(id){if(["previous-painting","next-painting","pause-paintings"].includes(id))get("dialog-content").emit("click",{target:get(id)});else get(id).emit("click");},
    action(name){actions.find(element=>element.dataset.action===name).emit("click");},
    preference(id,dataset={}){const button=new Element(id);button.dataset=dataset;get("dialog-content").emit("click",{target:button});},
    hidden(value){document.hidden=value;document.emit("visibilitychange");},
    tick(ms){while(ms>0){const delta=Math.min(ms,250);timestamp+=delta;ms-=delta;nextFrame(timestamp);}},
    load(id){const layer=get(id);layer.complete=true;layer.naturalWidth=1536;layer.emit("load");},
    fail(id){const layer=get(id);layer.complete=true;layer.naturalWidth=0;layer.emit("error");}
  };
}
async function settle(){await Promise.resolve();await Promise.resolve();}
let checks=0;
async function check(name,run){await run();checks++;process.stdout.write(`PASS ${name}\n`);}

(async()=>{
  await check("failed visible candidate keeps the old painting",async()=>{
    const h=harness();h.preloads();h.click("next-painting");h.tick(3000);
    assert.equal(h.state().currentPainting,3);assert.equal(h.state().transitionReady,false);assert.equal(h.get("painting-a").style.opacity,"1");
    h.fail("painting-b");h.tick(16000);
    assert.equal(h.state().currentPainting,3);assert.equal(h.state().transitioning,false);assert.equal(h.get("painting-a").style.opacity,"1");
    assert.equal(h.state().paintingStatuses[3],"failed");
    h.click("next-painting");assert.equal(h.state().pendingPainting,5);h.load("painting-b");await settle();h.tick(1000);
    assert.equal(h.state().currentPainting,5);
  });
  await check("decode failure rolls back an otherwise loaded target",async()=>{
    const h=harness();h.preloads();h.get("painting-b").decode=()=>Promise.reject(new Error("decode failure"));
    h.click("next-painting");h.load("painting-b");await settle();h.tick(16000);
    assert.equal(h.state().currentPainting,3);assert.equal(h.state().transitioning,false);assert.equal(h.get("painting-a").style.opacity,"1");
  });
  await check("failure during crossfade restores the full old layer",async()=>{
    const h=harness();h.preloads();h.click("next-painting");h.load("painting-b");await settle();h.tick(400);
    assert.equal(h.state().transitionReady,true);assert.ok(Number(h.get("painting-a").style.opacity)<1);
    h.fail("painting-b");assert.equal(h.get("painting-a").style.opacity,"1");assert.equal(h.state().currentPainting,3);
  });
  await check("stale decode rejection cannot cancel the replacement request",async()=>{
    const h=harness();h.preloads();let rejectOld;
    h.get("painting-b").decode=()=>new Promise((_,reject)=>{rejectOld=reject;});
    h.click("next-painting");h.load("painting-b");h.click("previous-painting");
    assert.equal(h.state().pendingPainting,2);rejectOld(new Error("superseded decode"));await settle();
    assert.equal(h.state().pendingPainting,2);assert.equal(h.state().paintingStatuses[1],"loaded");assert.equal(h.state().paintingStatuses[3],"loaded");
    h.get("painting-b").decode=()=>Promise.resolve();h.load("painting-b");await settle();h.tick(1000);assert.equal(h.state().currentPainting,2);
  });
  await check("manual navigation and automatic timing use the real sequence",async()=>{
    const h=harness();h.preloads();h.click("previous-painting");h.load("painting-b");await settle();h.tick(1000);assert.equal(h.state().currentPainting,2);
    h.click("next-painting");h.load("painting-a");await settle();h.tick(1000);assert.equal(h.state().currentPainting,3);
    const holdLeft=h.state().holdDuration-h.state().holdElapsed;h.tick(holdLeft-1);assert.equal(h.state().transitioning,false);
    h.tick(1);assert.equal(h.state().pendingPainting,4);assert.equal(h.state().transitionReady,false);
    h.load("painting-b");await settle();h.tick(h.state().fadeDuration-1);assert.equal(h.state().currentPainting,3);
    h.tick(1);assert.equal(h.state().currentPainting,4);
  });
  await check("hidden time does not advance the hold or the fade",async()=>{
    const h=harness();h.preloads();h.tick(10000);const held=h.state().holdElapsed;
    h.hidden(true);h.tick(180000);assert.equal(h.state().holdElapsed,held);
    h.hidden(false);h.tick(250);assert.equal(h.state().holdElapsed,held);
    h.click("next-painting");h.load("painting-b");await settle();h.tick(250);const fade=h.state().fadeElapsed;
    h.hidden(true);h.tick(180000);assert.equal(h.state().fadeElapsed,fade);assert.equal(h.state().currentPainting,3);
    h.hidden(false);h.tick(250);assert.equal(h.state().fadeElapsed,fade);
    const waiting=harness();waiting.preloads();waiting.click("next-painting");waiting.hidden(true);waiting.load("painting-b");await settle();waiting.tick(180000);
    assert.equal(waiting.state().currentPainting,3);assert.equal(waiting.state().fadeElapsed,0);assert.equal(waiting.state().transitionReady,true);
    waiting.hidden(false);waiting.tick(1000);assert.equal(waiting.state().currentPainting,3);waiting.tick(250);assert.equal(waiting.state().currentPainting,4);
  });
  await check("reduced motion stops timing and publishes only decoded manual images",async()=>{
    const h=harness({reduced:true});h.preloads();h.tick(180000);assert.equal(h.state().holdElapsed,0);assert.equal(h.state().currentPainting,3);
    h.click("next-painting");assert.equal(h.state().currentPainting,3);h.load("painting-b");await settle();assert.equal(h.state().currentPainting,4);assert.equal(h.state().transitioning,false);
    h.preference("reduce-motion");assert.equal(h.state().reducedMotion,false);h.tick(250);assert.ok(h.state().holdElapsed>0);
    h.click("pause-paintings");const held=h.state().holdElapsed;h.tick(180000);assert.equal(h.state().holdElapsed,held);
  });
  await check("failed current image can use another decoded painting",async()=>{
    const h=harness();h.preloads();h.fail("painting-a");assert.equal(h.state().pendingPainting,4);
    h.load("painting-b");await settle();assert.equal(h.state().currentPainting,4);assert.equal(h.get("painting-b").style.opacity,"1");
  });
  await check("all failed images retain a dark fallback without inventing success",async()=>{
    const h=harness();h.preloads(false);h.fail("painting-a");h.fail("painting-b");h.tick(180000);
    assert.equal(h.state().transitioning,false);assert.ok(h.state().paintingStatuses.every(value=>value==="failed"));
    assert.match(h.get("announcer").textContent,/油画未能加载/);
  });
  await check("three locale dictionaries match and no world API is required",async()=>{
    const match=source.match(/const messages = ([\s\S]+?);\n  const preferenceKey/);assert.ok(match);
    const messages=vm.runInNewContext(`(${match[1]})`);const keys=Object.keys(messages.zh).sort();
    for(const locale of ["zh","en","de"]){
      assert.deepEqual(Object.keys(messages[locale]).sort(),keys);
      for(const key of keys){assert.ok(messages[locale][key]);assert.deepEqual((messages[locale][key].match(/\{[^}]+\}/g)||[]).sort(),(messages.zh[key].match(/\{[^}]+\}/g)||[]).sort());}
      const h=harness();h.preference("locale",{locale});assert.equal(h.state().locale,locale);assert.equal(h.state().nativeWorldConnected,false);
    }
    for(const [,key] of html.matchAll(/data-(?:i18n|label)="([^"]+)"/g))assert.ok(messages.zh[key],`Missing translation: ${key}`);
  });
  await check("initial page contains exactly five pure text actions in one lower-right menu",async()=>{
    const initial=html.replace(/<dialog\b[\s\S]*?<\/dialog>/g,"").replace(/<template\b[\s\S]*?<\/template>/g,"");
    const navigation=initial.match(/<nav class="main-nav"[^>]*>([\s\S]*?)<\/nav>/);assert.ok(navigation);
    const allButtons=[...initial.matchAll(/<button\b[^>]*>([\s\S]*?)<\/button>/g)];assert.equal(allButtons.length,5);
    assert.equal([...navigation[1].matchAll(/<button\b/g)].length,5);
    for(const button of allButtons){assert.doesNotMatch(button[1],/<(?:svg|img|span|i)\b/);assert.ok(button[1].trim());}
    assert.match(initial,/<h1>SONNHEIDE<\/h1>/);assert.doesNotMatch(initial,/<(?:header|footer)\b/);
    assert.doesNotMatch(initial,/class="(?:eyebrow|tagline|painting-caption|prototype-label|utilities|crest|monogram)"/);
    const navigationStyle=css.match(/\.main-nav\{([^}]+)\}/);assert.ok(navigationStyle);
    assert.match(navigationStyle[1],/right:/);assert.match(navigationStyle[1],/bottom:/);assert.match(navigationStyle[1],/align-items:flex-end/);
    assert.match(css,/\.menu-action\{[^}]*text-align:right/);
  });
  await check("continue remains disabled and disclosures appear only on invocation",async()=>{
    const h=harness();h.action("continue");assert.equal(h.state().openPanel,null);
    h.action("new");assert.equal(h.state().openPanel,"new");assert.match(h.get("dialog-content").children[0].textContent,/尚未接入/);
    h.action("settings");assert.equal(h.state().openPanel,"settings");
    h.preference("about",{action:"about"});assert.equal(h.state().openPanel,"about");assert.match(h.get("dialog-content").children[0].textContent,/预览/);
  });
  process.stdout.write(`${checks} menu preview checks passed; browser visual/layout review remains separate.\n`);
})().catch(error=>{process.stderr.write(`${error.stack}\n`);process.exitCode=1;});
