"use strict";

// Standalone presentation prototype. No World, simulation, save or quit command is issued here.
(() => {
  const messages = {
  "zh": {
    "title": "Sonnheide — 主页面",
    "mainMenu": "主菜单",
    "new": "创建世界",
    "continue": "继续",
    "load": "载入世界",
    "settings": "设置",
    "about": "关于",
    "quit": "退出",
    "language": "语言",
    "close": "关闭",
    "noSaveExplanation": "继续：暂无已保存世界。",
    "previousPainting": "上一幅",
    "nextPainting": "下一幅",
    "pausePaintings": "暂停",
    "resumePaintings": "播放",
    "paintingChanged": "当前油画：{n} / 5",
    "motionTitle": "油画动画",
    "reduceMotion": "减弱动画",
    "motionDescription": "关闭自动换画与移动，仍可手动换画。",
    "temporaryPreferences": "浏览器未允许保存偏好，本次设置仅在当前页面有效。",
    "newTitle": "创建世界",
    "newBody": "真实世界创建尚未接入此预览。",
    "newDetail": "正式版本将从真实地表创建人口与建筑均为零的世界。你可以先查看独立的世界交互示例。",
    "showExample": "查看交互示例",
    "back": "返回",
    "loadTitle": "载入世界",
    "loadBody": "此预览尚未接入原生存档，暂无可载入的世界。",
    "loadDetail": "交互示例使用独立演示数据，不是真实存档。",
    "aboutTitle": "关于 Sonnheide",
    "aboutBody": "这是主页面的视觉与交互预览。真实世界创建、原生渲染和存档仍在开发中。",
    "aboutDetail": "五幅背景油画由你提供。界面语言与动画设置仅保存在此浏览器，不改变模拟世界。",
    "quitTitle": "退出",
    "quitBody": "此浏览器预览没有运行真实世界。关闭标签页即可退出预览。",
    "failedPaintings": "部分油画未能加载，保留可用油画与深色背景。",
    "allPaintingsFailed": "油画未能加载，使用深色背景。"
  },
  "en": {
    "title": "Sonnheide — Main menu",
    "mainMenu": "Main menu",
    "new": "New world",
    "continue": "Continue",
    "load": "Load world",
    "settings": "Settings",
    "about": "About",
    "quit": "Quit",
    "language": "Language",
    "close": "Close",
    "noSaveExplanation": "Continue: no saved world is available.",
    "previousPainting": "Previous",
    "nextPainting": "Next",
    "pausePaintings": "Pause",
    "resumePaintings": "Play",
    "paintingChanged": "Current painting: {n} / 5",
    "motionTitle": "Painting animation",
    "reduceMotion": "Reduce motion",
    "motionDescription": "Stop automatic changes and movement. Manual changes remain available.",
    "temporaryPreferences": "This browser did not allow preferences to be saved. These settings apply only to this page.",
    "newTitle": "New world",
    "newBody": "Real world creation is not connected to this preview yet.",
    "newDetail": "The native game will begin with real terrain and no people or buildings. You can explore the separate interaction example.",
    "showExample": "View the interaction example",
    "back": "Back",
    "loadTitle": "Load world",
    "loadBody": "This preview is not connected to native saves. No world is available to load.",
    "loadDetail": "The interaction example uses separate demonstration data, not a real save.",
    "aboutTitle": "About Sonnheide",
    "aboutBody": "This is a visual and interaction preview of the main menu. Real world creation, native rendering and saves are still in development.",
    "aboutDetail": "You supplied the five background paintings. Language and animation preferences stay in this browser and do not change a simulated world.",
    "quitTitle": "Quit",
    "quitBody": "No real world is running in this browser preview. Close this tab to leave the preview.",
    "failedPaintings": "Some paintings could not be loaded. Available paintings and the dark background remain.",
    "allPaintingsFailed": "The paintings could not be loaded. Using the dark background."
  },
  "de": {
    "title": "Sonnheide — Hauptmenü",
    "mainMenu": "Hauptmenü",
    "new": "Neue Welt",
    "continue": "Fortsetzen",
    "load": "Welt laden",
    "settings": "Einstellungen",
    "about": "Über Sonnheide",
    "quit": "Beenden",
    "language": "Sprache",
    "close": "Schließen",
    "noSaveExplanation": "Fortsetzen: Es ist keine gespeicherte Welt verfügbar.",
    "previousPainting": "Zurück",
    "nextPainting": "Weiter",
    "pausePaintings": "Pause",
    "resumePaintings": "Abspielen",
    "paintingChanged": "Aktuelles Gemälde: {n} / 5",
    "motionTitle": "Gemäldeanimation",
    "reduceMotion": "Bewegung reduzieren",
    "motionDescription": "Automatischen Wechsel und Bewegung stoppen. Manuelles Wechseln bleibt möglich.",
    "temporaryPreferences": "Der Browser erlaubt das Speichern der Vorlieben nicht. Diese Einstellungen gelten nur für diese Seite.",
    "newTitle": "Neue Welt",
    "newBody": "Die Erstellung echter Welten ist noch nicht mit dieser Vorschau verbunden.",
    "newDetail": "Das native Spiel beginnt mit echtem Gelände, ohne Menschen oder Gebäude. Du kannst das separate Interaktionsbeispiel ansehen.",
    "showExample": "Interaktionsbeispiel ansehen",
    "back": "Zurück",
    "loadTitle": "Welt laden",
    "loadBody": "Diese Vorschau ist nicht mit nativen Spielständen verbunden. Es ist keine Welt zum Laden verfügbar.",
    "loadDetail": "Das Interaktionsbeispiel verwendet eigene Beispieldaten, keinen echten Spielstand.",
    "aboutTitle": "Über Sonnheide",
    "aboutBody": "Dies ist eine visuelle und interaktive Vorschau des Hauptmenüs. Echte Welterstellung, natives Rendering und Spielstände sind noch in Entwicklung.",
    "aboutDetail": "Die fünf Hintergrundgemälde wurden von dir bereitgestellt. Sprach- und Animationsvorlieben bleiben in diesem Browser und verändern keine simulierte Welt.",
    "quitTitle": "Beenden",
    "quitBody": "In dieser Browservorschau läuft keine echte Welt. Schließe diesen Tab, um die Vorschau zu verlassen.",
    "failedPaintings": "Einige Gemälde konnten nicht geladen werden. Verfügbare Bilder und der dunkle Hintergrund bleiben erhalten.",
    "allPaintingsFailed": "Die Gemälde konnten nicht geladen werden. Der dunkle Hintergrund wird verwendet."
  }
};
  const preferenceKey = "sonnheide.menu-preview.preferences.v1";
  const supportedLocales = Object.keys(messages);
  const reducedQuery = window.matchMedia("(prefers-reduced-motion: reduce)");
  let storageAvailable = true;
  let saved = {};
  try { const value = JSON.parse(localStorage.getItem(preferenceKey) || "{}"); if (value && typeof value === "object" && !Array.isArray(value)) saved = value; } catch (_) { storageAvailable = false; }
  const queryLocale = new URLSearchParams(location.search).get("lang");
  let locale = supportedLocales.includes(queryLocale) ? queryLocale : supportedLocales.includes(saved.locale) ? saved.locale : "zh";
  let reducedMotion = typeof saved.reducedMotion === "boolean" ? saved.reducedMotion : reducedQuery.matches;
  let userSetMotion = typeof saved.reducedMotion === "boolean";
  let paintingPaused = false;
  let openPanel = null;
  const dialog = document.getElementById("menu-dialog");
  const dialogContent = document.getElementById("dialog-content");
  const layers = [document.getElementById("painting-a"), document.getElementById("painting-b")];
  const layerPaintings = [2, 2];
  const paintings = Array.from({length:5}, (_, i) => ({index:i,src:`../../assets/source/ui/paintings/painting-${String(i+1).padStart(2,"0")}.jpg`,status:"pending"}));
  const positions = ["58% 47%", "57% 42%", "55% 40%", "60% 48%", "57% 46%"];
  let currentPainting = 2;
  let frontLayer = 0;
  let elapsed = 0;
  let transition = null;
  let lastTimestamp = 0;
  let fallbackAnnounced = false;
  const holdDuration = 68000;
  const fadeDuration = 12000;
  // Local presentation randomness never shares a clock or RNG with a simulated World.
  // Both decoded layers follow one slow path so changing paintings cannot restart the drift.
  const motionDirections = [
    {name:"lower-left",x:-1,y:1}, {name:"upper-right",x:1,y:-1},
    {name:"lower-right",x:1,y:1}, {name:"upper-left",x:-1,y:-1}
  ];
  const motionLimits={x:.55,y:.4};
  const zoomDuration = 240000;
  const layerZoomElapsed = [0,0];
  let previousMotionDirection = null;
  function nextMotionTarget(from) {
    // Select an actual diagonal displacement, not a fixed corner that could produce a
    // horizontal/vertical segment. Exclude directions with too little room to move.
    const candidates=motionDirections.filter(direction=>direction.name!==previousMotionDirection && motionLimits.x-direction.x*from.x>=.075 && motionLimits.y-direction.y*from.y>=.055);
    const direction=candidates[Math.floor(Math.random()*candidates.length)];
    previousMotionDirection=direction.name;
    return {name:direction.name,x:from.x+direction.x*(motionLimits.x-direction.x*from.x)*(.35+Math.random()*.4),y:from.y+direction.y*(motionLimits.y-direction.y*from.y)*(.35+Math.random()*.4)};
  }
  const paintingMotion={from:{x:0,y:0},target:nextMotionTarget({x:0,y:0}),elapsed:0,duration:140000+Math.random()*60000};
  let motionCanAdvance=!document.hidden && !reducedMotion && !paintingPaused;
  let motionClockNeedsReset=false;
  const smoothMotion=fraction=>fraction*fraction*fraction*(fraction*(fraction*6-15)+10);
  function motionPose() {
    const fraction=smoothMotion(paintingMotion.elapsed/paintingMotion.duration);
    return {x:paintingMotion.from.x+(paintingMotion.target.x-paintingMotion.from.x)*fraction,y:paintingMotion.from.y+(paintingMotion.target.y-paintingMotion.from.y)*fraction};
  }
  function renderPaintingMotion() {
    const pose=motionPose();
    layers.forEach((layer,index)=>{
      const scale=1.018+.018*smoothMotion(layerZoomElapsed[index]/zoomDuration);
      layer.style.transform=`translate3d(${pose.x.toFixed(6)}%,${pose.y.toFixed(6)}%,0) scale(${scale.toFixed(6)})`;
    });
  }
  function advancePaintingMotion(delta) {
    paintingMotion.elapsed+=delta;
    while(paintingMotion.elapsed>=paintingMotion.duration){
      paintingMotion.elapsed-=paintingMotion.duration;
      paintingMotion.from={x:paintingMotion.target.x,y:paintingMotion.target.y};
      paintingMotion.target=nextMotionTarget(paintingMotion.from);paintingMotion.duration=140000+Math.random()*60000;
    }
    layerZoomElapsed[frontLayer]=Math.min(layerZoomElapsed[frontLayer]+delta,zoomDuration);
    if(transition && transition.ready)layerZoomElapsed[transition.targetLayer]=Math.min(layerZoomElapsed[transition.targetLayer]+delta,zoomDuration);
    renderPaintingMotion();
  }

  function t(key) { return messages[locale][key] || key; }
  function announce(key, replacements={}) { let value=t(key); for(const [name,replacement] of Object.entries(replacements)) value=value.replace(`{${name}}`,String(replacement)); document.getElementById("announcer").textContent=value; }
  function persist() { try { localStorage.setItem(preferenceKey, JSON.stringify({locale,reducedMotion}));storageAvailable=true; } catch (_) { storageAvailable=false; } }
  function translate(root=document) {
    for(const element of root.querySelectorAll("[data-i18n]")) element.textContent=t(element.dataset.i18n);
    for(const element of root.querySelectorAll("[data-label]")) { const label=t(element.dataset.label); element.setAttribute("aria-label",label); if(element.matches("button")) element.title=label; }
  }
  function updateControls() {
    const canAdvance=!document.hidden && !reducedMotion && !paintingPaused;
    if(canAdvance!==motionCanAdvance){motionCanAdvance=canAdvance;motionClockNeedsReset=true;}
    document.body.classList.toggle("reduced-motion", reducedMotion);
    document.body.classList.toggle("motion-enabled", !reducedMotion);
    document.body.classList.toggle("motion-paused",paintingPaused || document.hidden || reducedMotion);
    const pause = document.getElementById("pause-paintings");
    if(pause){
      pause.dataset.i18n=(paintingPaused || reducedMotion) ? "resumePaintings" : "pausePaintings";
      pause.setAttribute("aria-pressed",String(paintingPaused || reducedMotion));
      pause.textContent=t(pause.dataset.i18n);
    }
    const motion=document.getElementById("reduce-motion");if(motion)motion.setAttribute("aria-pressed",String(reducedMotion));
    for(const button of dialogContent.querySelectorAll("[data-locale]"))button.setAttribute("aria-pressed",String(button.dataset.locale===locale));
    const note=document.getElementById("settings-storage-note");if(note){note.hidden=storageAvailable;note.textContent=storageAvailable?"":t("temporaryPreferences");}
  }
  function applyLocale() {
    document.documentElement.lang=locale==="zh"?"zh-CN":locale;
    document.title=t("title");translate();
    if(openPanel && openPanel!=="settings")renderPanel(openPanel);
    else if(openPanel==="settings")document.getElementById("dialog-title").textContent=t("settings");
    updateControls();
  }
  function renderPanel(kind) {
    const specification={new:["newTitle","newBody","newDetail"],continue:["loadTitle","loadBody","loadDetail"],load:["loadTitle","loadBody","loadDetail"],about:["aboutTitle","aboutBody","aboutDetail"],quit:["quitTitle","quitBody",null]}[kind];
    document.getElementById("dialog-title").textContent=t(specification[0]);
    dialogContent.replaceChildren();
    for(const key of specification.slice(1).filter(Boolean)){const p=document.createElement("p");p.className="dialog-copy";p.textContent=t(key);dialogContent.appendChild(p);}
    const actions=document.createElement("div");actions.className="dialog-actions";
    if(kind==="new" || kind==="load"){const link=document.createElement("a");link.href="world-inspector.html";link.className="text-action";link.textContent=t("showExample");actions.appendChild(link);}
    const back=document.createElement("button");back.type="button";back.className="text-action secondary";back.textContent=t("back");back.addEventListener("click",()=>dialog.close());actions.appendChild(back);
    dialogContent.appendChild(actions);
  }
  function openDialog(kind) {
    openPanel=kind;
    if(kind==="settings"){
      document.getElementById("dialog-title").textContent=t("settings");
      dialogContent.replaceChildren(document.getElementById("settings-template").content.cloneNode(true));translate(dialogContent);updateControls();
    }else renderPanel(kind);
    if(!dialog.open)dialog.showModal();
    document.getElementById("close-dialog").focus();
  }
  for(const button of document.querySelectorAll(".main-nav [data-action]"))button.addEventListener("click",()=>{if(!button.disabled)openDialog(button.dataset.action);});
  document.getElementById("close-dialog").addEventListener("click",()=>dialog.close());
  dialog.addEventListener("close",()=>{openPanel=null;});
  dialog.addEventListener("click",event=>{if(event.target===dialog){const rectangle=dialog.getBoundingClientRect();if(event.clientX<rectangle.left||event.clientX>rectangle.right||event.clientY<rectangle.top||event.clientY>rectangle.bottom)dialog.close();}});
  dialogContent.addEventListener("click",event=>{
    const button=event.target.closest("button");if(!button)return;
    if(button.dataset.action){openDialog(button.dataset.action);return;}
    if(button.dataset.locale){locale=button.dataset.locale;persist();applyLocale();}
    if(button.id==="reduce-motion"){reducedMotion=!reducedMotion;userSetMotion=true;if(!finishTransition())cancelTransition();persist();updateControls();}
    if(button.id==="previous-painting")choosePainting(-1,true);
    if(button.id==="next-painting")choosePainting(1,true);
    if(button.id==="pause-paintings"){
      if(reducedMotion){reducedMotion=false;userSetMotion=true;paintingPaused=false;persist();}else paintingPaused=!paintingPaused;
      if(!finishTransition())cancelTransition();elapsed=0;updateControls();
    }
  });

  function cancelTransition() {
    if(!transition)return;
    layers[transition.targetLayer].style.opacity="0";
    layers[frontLayer].style.opacity="1";
    transition=null;elapsed=0;
  }
  function finishTransition() {
    // A preload is insufficient: publish only the actual visible layer after decoding.
    if(!transition || !transition.ready)return false;
    if(layers[transition.targetLayer].classList.contains("is-failed")){cancelTransition();return false;}
    const manual=transition.manual;
    layers[transition.targetLayer].style.opacity="1";layers[frontLayer].style.opacity="0";
    frontLayer=transition.targetLayer;currentPainting=transition.index;transition=null;elapsed=0;
    if(manual)announce("paintingChanged",{n:currentPainting+1});
    return true;
  }
  function failLayer(layerIndex) {
    layers[layerIndex].classList.add("is-failed");
    paintings[layerPaintings[layerIndex]].status="failed";
    if(transition && transition.targetLayer===layerIndex)cancelTransition();
    if(layerIndex===frontLayer)choosePainting(1,true);
    checkFailures();
  }
  async function prepareTransition(request) {
    if(transition!==request || request.ready || request.decoding)return;
    const layer=layers[request.targetLayer];
    if(!layer.complete || layer.naturalWidth===0)return;
    request.decoding=true;
    try { if(typeof layer.decode==="function")await layer.decode(); }
    catch (_) { if(transition===request)failLayer(request.targetLayer);return; }
    // Replaced requests, including their asynchronous decode errors, cannot publish a newer layer.
    if(transition!==request)return;
    if(!layer.complete || layer.naturalWidth===0 || layer.classList.contains("is-failed")){failLayer(request.targetLayer);return;}
    request.ready=true;
    if(!document.hidden && (reducedMotion || paintingPaused || layers[frontLayer].classList.contains("is-failed")))finishTransition();
  }
  function choosePainting(direction=1, manual=false) {
    if(transition && !finishTransition())cancelTransition();
    let next=currentPainting;
    for(let step=1;step<=paintings.length;step++){
      const candidate=(currentPainting+direction*step+paintings.length*2)%paintings.length;
      if(paintings[candidate].status==="loaded"){next=candidate;break;}
    }
    if(next===currentPainting)return;
    const targetLayer=1-frontLayer;
    layerPaintings[targetLayer]=next;
    // Only the hidden candidate gets a new zoom clock; cancellation preserves the old front.
    layerZoomElapsed[targetLayer]=0;renderPaintingMotion();
    transition={index:next,targetLayer,elapsed:0,duration:manual?900:fadeDuration,manual,ready:false,decoding:false};
    const request=transition;
    layers[targetLayer].classList.remove("is-failed");layers[targetLayer].style.objectPosition=positions[next];layers[targetLayer].style.opacity="0";
    layers[targetLayer].src=paintings[next].src;
    prepareTransition(request);
  }
  function checkFailures() {
    if(paintings.some(p=>p.status==="pending")||fallbackAnnounced)return;
    if(paintings.some(p=>p.status==="failed")){fallbackAnnounced=true;announce(paintings.every(p=>p.status==="failed")?"allPaintingsFailed":"failedPaintings");}
  }
  layers.forEach((layer,index)=>{
    layer.addEventListener("error",()=>failLayer(index));
    layer.addEventListener("load",()=>{if(transition && transition.targetLayer===index)prepareTransition(transition);});
  });
  for(const painting of paintings){
    const image=new Image();image.onload=()=>{if(painting.status!=="failed")painting.status="loaded";if(paintings[currentPainting].status==="failed"&&!transition)choosePainting(1,true);checkFailures();};image.onerror=()=>{painting.status="failed";if(painting.index===currentPainting)choosePainting(1,true);checkFailures();};image.src=painting.src;
  }
  document.addEventListener("visibilitychange",()=>{lastTimestamp=0;if(!document.hidden && transition && transition.ready && (reducedMotion || paintingPaused || layers[frontLayer].classList.contains("is-failed")))finishTransition();updateControls();});
  const onMotionPreference=()=>{if(!userSetMotion){reducedMotion=reducedQuery.matches;if(!finishTransition())cancelTransition();updateControls();}};
  if(reducedQuery.addEventListener)reducedQuery.addEventListener("change",onMotionPreference);else reducedQuery.addListener(onMotionPreference);
  function frame(timestamp) {
    const delta=lastTimestamp?Math.min(timestamp-lastTimestamp,250):0;lastTimestamp=timestamp;
    const motionDelta=motionClockNeedsReset?0:delta;motionClockNeedsReset=false;
    if(!document.hidden && !reducedMotion && !paintingPaused){
      advancePaintingMotion(motionDelta);
      if(transition && transition.ready){
        transition.elapsed+=delta;const fraction=Math.min(transition.elapsed/transition.duration,1);const eased=fraction*fraction*(3-2*fraction);
        layers[transition.targetLayer].style.opacity=String(eased);layers[frontLayer].style.opacity=String(1-eased);
        if(fraction===1)finishTransition();
      }else if(!transition){elapsed+=delta;if(elapsed>=holdDuration){elapsed=0;choosePainting(1);}}
    }
    requestAnimationFrame(frame);
  }
  // Read-only metadata for browser review; intentionally exposes no world mutation API.
  window.SonnheideMenuPreview=Object.freeze({getState:()=>({locale,reducedMotion,paintingPaused,currentPainting:currentPainting+1,paintingStatuses:paintings.map(p=>p.status),transitioning:Boolean(transition),pendingPainting:transition?transition.index+1:null,transitionReady:Boolean(transition && transition.ready),holdElapsed:elapsed,fadeElapsed:transition?transition.elapsed:0,motion:{direction:paintingMotion.target.name,from:{...paintingMotion.from},target:{x:paintingMotion.target.x,y:paintingMotion.target.y},segmentElapsed:paintingMotion.elapsed,segmentDuration:paintingMotion.duration,phase:paintingMotion.elapsed/paintingMotion.duration,translation:motionPose(),zoomElapsed:layerZoomElapsed.slice(),scales:layerZoomElapsed.map(value=>1.018+.018*smoothMotion(value/zoomDuration)),zoomDuration},openPanel,storageAvailable,holdDuration,fadeDuration,nativeWorldConnected:false})});
  layers[frontLayer].style.opacity="1";layers[1-frontLayer].style.opacity="0";
  layers[frontLayer].style.objectPosition=positions[currentPainting];
  applyLocale();renderPaintingMotion();requestAnimationFrame(frame);
})();
