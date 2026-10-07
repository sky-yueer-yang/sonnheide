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

function harness({reduced=false,random=null}={}) {
  const elements=new Map();const get=id=>{if(!elements.has(id))elements.set(id,new Element(id));return elements.get(id);};
  const actions=["new","continue","load","settings","quit"].map(action=>{const element=new Element();element.dataset.action=action;element.disabled=action==="continue";return element;});
  const document=new Element("document","document");document.hidden=false;document.body=new Element("body","body");document.documentElement=new Element("html","html");
  document.getElementById=get;document.querySelectorAll=selector=>selector===".main-nav [data-action]"?actions:[];
  document.createElement=tag=>new Element("",tag);document.createTextNode=text=>({textContent:text});
  get("settings-template").content={cloneNode:()=>new Element("settings-clone","div")};
  const images=[];class Preload {constructor(){images.push(this);}}
  const preferences=new Map();const localStorage={getItem:key=>preferences.get(key)||null,setItem:(key,value)=>preferences.set(key,value)};
  const media={matches:reduced,addEventListener(){}};const window={matchMedia:()=>media};
  let nextFrame=null;let timestamp=1;let randomCalls=0;let randomSeed=1337;
  const presentationMath=Object.create(Math);presentationMath.random=()=>{randomCalls++;if(random)return random();randomSeed=(Math.imul(randomSeed,1664525)+1013904223)>>>0;return randomSeed/4294967296;};
  const context={document,window,location:{search:""},localStorage,Image:Preload,URLSearchParams,Math:presentationMath,requestAnimationFrame:fn=>{nextFrame=fn;}};
  for(const forbidden of ["World","world","simulation","nativeBridge","fetch","XMLHttpRequest"])
    Object.defineProperty(context,forbidden,{get(){throw new Error(`Unexpected world/network access: ${forbidden}`);}});
  vm.runInNewContext(source,context,{filename:"main-menu.js"});
  nextFrame(timestamp);
  return {
    get,document,actions,images,randomCalls:()=>randomCalls,
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
  await check("slow local drift uses four actual diagonals without repeated directions or boundary jumps",async()=>{
    const h=harness();assert.equal(h.get("painting-a").style.transform,"translate3d(0.000000%,0.000000%,0) scale(1.018000)");
    const firstDraws=h.randomCalls();h.tick(20000);assert.equal(h.randomCalls(),firstDraws,"random targets must not be selected every frame");
    const directions=[];
    function assertBounds(){
      const motion=h.state().motion;
      assert.ok(motion.segmentDuration>=140000 && motion.segmentDuration<=200000);
      assert.ok(motion.segmentElapsed>=0 && motion.segmentElapsed<motion.segmentDuration);
      assert.ok(Math.abs(motion.translation.x)<=.55 && Math.abs(motion.translation.y)<=.4);
      for(const scale of motion.scales)assert.ok(scale>=1.018 && scale<=1.036);
      for(const elapsed of motion.zoomElapsed)assert.ok(elapsed>=0 && elapsed<=motion.zoomDuration);
      return motion;
    }
    for(let segment=0;segment<40;segment++){
      const start=assertBounds();directions.push(start.direction);
      const signs={"lower-left":[-1,1],"upper-right":[1,-1],"lower-right":[1,1],"upper-left":[-1,-1]}[start.direction];
      assert.equal(Math.sign(start.target.x-start.from.x),signs[0]);assert.equal(Math.sign(start.target.y-start.from.y),signs[1]);
      assert.ok(Math.abs(start.target.x-start.from.x)>=.075*.35 && Math.abs(start.target.y-start.from.y)>=.055*.35,"every segment must visibly move both axes");
      if(segment)assert.notEqual(directions[segment],directions[segment-1]);
      let left=start.segmentDuration-start.segmentElapsed;
      while(left>10001){h.tick(10000);assertBounds();left-=10000;}
      h.tick(left-1);const before=assertBounds();h.tick(2);const after=assertBounds();
      assert.notEqual(after.direction,before.direction);
      assert.ok(Math.abs(after.translation.x-before.translation.x)<.000001 && Math.abs(after.translation.y-before.translation.y)<.000001,"segment junction must be continuous");
      assert.equal(h.get("painting-a").style.transform.split(" scale")[0],h.get("painting-b").style.transform.split(" scale")[0],"both layers share the same drift");
    }
    assert.equal(new Set(directions).size,4,"the local random sequence must cover all four actual directions");
    assert.equal(h.state().motion.scales[0],1.036);
    // Extreme legal random draws exercise directions rejected near the boundary.
    for(const draw of [0,1-Number.EPSILON]){
      const edge=harness({random:()=>draw});
      for(let segment=0;segment<20;segment++){
        const before=edge.state().motion;edge.tick(before.segmentDuration-before.segmentElapsed+1);const after=edge.state().motion;
        assert.notEqual(after.direction,before.direction);
        assert.ok(Math.abs(after.translation.x)<=.55 && Math.abs(after.translation.y)<=.4);
        assert.ok(Math.abs(after.target.x-after.from.x)>=.075*.35 && Math.abs(after.target.y-after.from.y)>=.055*.35);
      }
    }
  });
  await check("pause, hidden and reduced motion freeze the exact pose and resume without catching up",async()=>{
    const initial=harness({reduced:true});initial.tick(240000);
    assert.equal(initial.get("painting-a").style.transform,"translate3d(0.000000%,0.000000%,0) scale(1.018000)");
    const h=harness();h.tick(50000);
    for(const gate of ["pause","hidden","reduced"]){
      const toggle=value=>gate==="hidden"?h.hidden(value):gate==="pause"?h.click("pause-paintings"):h.preference("reduce-motion");
      toggle(true);const frozen=h.state().motion;const transforms=[h.get("painting-a").style.transform,h.get("painting-b").style.transform];
      h.tick(400000);assert.deepEqual(h.state().motion,frozen);assert.deepEqual([h.get("painting-a").style.transform,h.get("painting-b").style.transform],transforms);
      toggle(false);h.tick(250);assert.deepEqual(h.state().motion,frozen,"first resumed frame must not accumulate gated time");
      h.tick(1000);assert.ok(h.state().motion.segmentElapsed>frozen.segmentElapsed);
    }
  });
  await check("new paintings reset only the hidden zoom clock and failures preserve the current pose",async()=>{
    const h=harness();h.preloads();h.tick(40000);
    const old=h.state().motion;const frontTransform=h.get("painting-a").style.transform;
    h.click("next-painting");
    assert.deepEqual(h.state().motion.translation,old.translation);assert.equal(h.state().motion.segmentElapsed,old.segmentElapsed);
    assert.equal(h.state().motion.zoomElapsed[0],old.zoomElapsed[0]);assert.equal(h.state().motion.zoomElapsed[1],0);
    assert.equal(h.get("painting-a").style.transform,frontTransform);
    const beforeLocale=h.state().motion;h.preference("locale",{locale:"de"});assert.deepEqual(h.state().motion,beforeLocale);
    h.load("painting-b");await settle();h.tick(400);
    const beforeFailure=h.state().motion;const beforeFailureTransform=h.get("painting-a").style.transform;
    h.fail("painting-b");assert.deepEqual(h.state().motion,beforeFailure);assert.equal(h.get("painting-a").style.transform,beforeFailureTransform);
    h.tick(10000);const beforeNew=h.state().motion;h.click("next-painting");h.load("painting-b");await settle();h.tick(1000);
    assert.equal(h.state().currentPainting,5);assert.equal(h.state().motion.zoomElapsed[0],beforeNew.zoomElapsed[0]+1000);
    assert.equal(h.state().motion.zoomElapsed[1],1000);assert.ok(h.state().motion.segmentElapsed>beforeNew.segmentElapsed);
    h.click("next-painting");assert.equal(h.state().motion.zoomElapsed[0],0);assert.equal(h.state().motion.zoomElapsed[1],1000);
  });
  await check("manual reduced-motion changes keep the shared drift static and use a stationary new image",async()=>{
    const h=harness();h.preloads();h.tick(40000);h.preference("reduce-motion");
    const frozen=h.state().motion;h.click("next-painting");h.load("painting-b");await settle();h.tick(240000);
    assert.equal(h.state().currentPainting,4);assert.equal(h.state().motion.segmentElapsed,frozen.segmentElapsed);
    assert.deepEqual(h.state().motion.translation,frozen.translation);assert.equal(h.state().motion.zoomElapsed[0],frozen.zoomElapsed[0]);assert.equal(h.state().motion.scales[1],1.018);
    const transform=h.get("painting-b").style.transform;h.preference("reduce-motion");h.tick(250);assert.equal(h.get("painting-b").style.transform,transform);
    const motion=h.state().motion;motion.translation.x=999;motion.zoomElapsed[1]=999;assert.notEqual(h.state().motion.translation.x,999);assert.notEqual(h.state().motion.zoomElapsed[1],999);
    h.tick(1000);assert.ok(h.state().motion.zoomElapsed[1]>0);
  });
  await check("three locale dictionaries match and no world API is required",async()=>{
    const match=source.match(/const messages = ([\s\S]+?);\r?\n  const preferenceKey/);assert.ok(match);
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
    assert.match(navigationStyle[1],/right:/);assert.match(navigationStyle[1],/top:/);assert.match(navigationStyle[1],/align-items:flex-end/);
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
