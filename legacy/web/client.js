/* Wirescape 0.5 frontend bridge. Patch editor runs in the unchanged template.
   All actual DSP stays in the Python host's one PortAudio callback. */
(() => {
'use strict';
const $ = (q,root=document)=>root.querySelector(q);
// Mirrors mapping.FRONT_OWNS; keep in sync with the host contract.
const FRONT_OWNS={MORPH:['A_LEVEL','B_LEVEL','A_MUTE','B_MUTE'],CHAOS:['JITTER','ROUGH_DEPTH','DRIFT'],FOLD:['BREAK','DRIVE_TYPE'],CUTOFF:['CUTOFF'],PITCH:['OFFSET'],DETUNE:['DETUNE','VOICES'],CLUSTER:['CLUSTER'],SUB:['SUB'],FM:['FM_RATIO'],ROUGH:['ROUGH_DEPTH'],JITTER:['JITTER'],DRIFT:['DRIFT'],FALL:['FALL'],COUPLE:['ROUTE_MODE','ROUTE_DEPTH'],BODY:['BODY_MIX'],TONE:['BODY_TONE'],FEEDBACK:['BODY_FEED'],RATE:['LFO_RATE'],MOTION:['LFO_DEPTH','LFO_SELF'],TAPE:['TAPE'],SPACE:['SPACE','SPACE_MIX'],LOOP:['LOOP_MIX','LOOP_FEEDBACK']};
let ws=null,schema=null,selected=null,connected=false,hostReady=false,audioRunning=false;
let lastTopology='',activeNotes=new Map(),presetCatalog=[];
const inspect=$('#inspectDrawer'),deepBody=$('#deepBody'),deepName=$('#deepName');
const inspectBtn=$('#inspectBtn'),deviceBtn=$('#deviceBtn'),devicePanel=$('#devicePanel');
const badge=$('#engineBadge'),audioBadge=$('#audioBadge'),loadBadge=$('#loadBadge');
const notice=t=>{ const el=$('#hostToast');el.textContent=t;el.classList.add('show');setTimeout(()=>el.classList.remove('show'),3500);};
function send(p){if(ws?.readyState===WebSocket.OPEN)ws.send(JSON.stringify(p));}
function data(){return window.__wirescapeTest?.state();}
function topology(p){return JSON.stringify({modules:p.modules.map(m=>[m.id,m.type]).sort((a,b)=>a[0].localeCompare(b[0])),connections:p.connections.map(c=>[c.from,c.to]).sort((a,b)=>a[0].localeCompare(b[0]))});}
function patchChanged(p,force=false){if(!p)return;const hash=topology(p);if(force||hash!==lastTopology){lastTopology=hash;send({type:'patch',state:p});}}
function control(id,name,value){
 const mod=patchModule(id);
 if(mod?.type==='chaos'&&mod.advanced){for(const key of FRONT_OWNS[name]||[])delete mod.advanced[key];}
 send({type:'control',id,name,value});
}
function mode(id,index){send({type:'mode',id,index});}
function monitor(id,on){send({type:'monitor',id,on});}
function selection(v){selected=v;if(v?.type==='module'){$('#deepLaunch').disabled=false;deepName.textContent=(data()?.modules.find(m=>m.id===v.id)?.type||'MODULE').toUpperCase()+' / '+v.id;}else{$('#deepLaunch').disabled=true;}if(inspect.classList.contains('visible'))renderDeep();}
function fillPresets(id){
 const select=document.querySelector(`[data-presets-for="${CSS.escape(id)}"]`);
 if(!select)return;
 const before=select.value;
 select.replaceChildren(new Option('Choose a preset…',''));
 for(const item of presetCatalog){
  const opt=new Option(`${item.kind==='factory'?'FACTORY':'USER'} / ${item.name}`,item.id);
  select.append(opt);
 }
 if([...select.options].some(x=>x.value===before))select.value=before;
}
function refreshPresets(){for(const m of data()?.modules||[])if(m.type==='chaos')fillPresets(m.id);}
function loadPreset(id,preset){send({type:'preset',id,preset});}
function savePreset(id){const m=patchModule(id);if(!m)return;
 const name=prompt('Save CHAOS preset as…');if(name===null)return;
 if(!name.trim()){notice('Enter a preset name.');return;}
 send({type:'preset_save',id,name:name.trim(),state:{values:m.values,advanced:m.advanced,raw:m.raw,mode:m.mode}});
}
function routingTargets(){const synths=(data()?.modules||[]).filter(m=>m.type==='chaos');const solos=synths.filter(m=>m.keyboardSolo);
 return (solos.length?solos:synths.filter(m=>m.keyboardArmed)).map(m=>m.id);}
function routingChanged(){
 // Preserve exact note owners, including when the panel was open or selection changes.
 const targets=new Set(routingTargets());
 for(const [note,ids] of activeNotes){for(const id of [...ids])if(!targets.has(id)){
  send({type:'note',id,note,on:false});ids.delete(id);
 }}
}
window.wirescapeBridge={patchChanged,control,mode,monitor,selection,openEngine:(id)=>openEngine(id),
 openModuleEditor:(id,index)=>openModuleEditor(id,index),
 fillPresets,loadPreset,savePreset,routingChanged};
function setStatus(info){
 audioRunning=!!info.running;
 audioBadge.textContent=audioRunning?'AUDIO / RUNNING':'AUDIO / OFF';
 audioBadge.classList.toggle('running',audioRunning);
 if(info.error)audioBadge.title='Audio error: '+info.error;
 const cpu=(100*(info.load||0));
 loadBadge.textContent=audioRunning?`${cpu.toFixed(0)}% DSP · ${info.xruns||0} XRUN`:'DSP / IDLE';
 const io=data()?.modules.filter(m=>m.type==='interface')||[];
 for(const mod of io){const root=document.querySelector(`[data-id="${CSS.escape(mod.id)}"]`);if(!root)continue;const fill=root.querySelectorAll('.meter-fill');if(fill.length>=2){
  fill[0].style.width=Math.min(100,(info.rms_l||0)*240)+'%';
  fill[1].style.width=Math.min(100,(info.rms_r||0)*240)+'%';
 }
 const disclaimer=root.querySelector('.io-disclaimer');if(disclaimer)disclaimer.textContent=audioRunning?'LIVE AUDIO METERS · HOST OUTPUT':'HOST IDLE · METERS INACTIVE';
 }
}
const loadedAssets=new Set();
async function loadModuleAssets(assets){
 for(const [id,spec] of Object.entries(assets)){
   for(const name of spec.assets||[]){
     const url=`/module-assets/${encodeURIComponent(id)}/${encodeURIComponent(name)}?v=${encodeURIComponent(spec.version||'1')}`;
     if(loadedAssets.has(url))continue;
     if(name.endsWith('.css')){
       const link=document.createElement('link');link.rel='stylesheet';link.href=url;document.head.append(link);
       loadedAssets.add(url);
     }else if(name.endsWith('.js')){
       await new Promise((resolve,reject)=>{const script=document.createElement('script');script.src=url;
         script.onload=resolve;script.onerror=()=>reject(new Error('Unable to load module UI '+id));document.head.append(script);});
       loadedAssets.add(url);
     }
   }
 }
}
function renderModuleCatalog(list){
 const panel=$('#moduleCatalog');if(!panel)return;panel.replaceChildren();
 if(!list.length){const p=document.createElement('p');p.className='inspect-caption';p.textContent='No module.json folders found. Drop a module into modules/<name>/ and press SCAN.';panel.append(p);return;}
 for(const m of list){
   const row=document.createElement('div');row.className='deep-param';
   const line=document.createElement('div');line.className='deep-param-meta';
   const title=document.createElement('strong');title.textContent=`${m.name} / ${m.version}`;
   const status=document.createElement('span');status.textContent=m.restart_required?'RESTART TO UPDATE':m.active?'ACTIVE':m.error?'ERROR':'NOT ENABLED';
   line.append(title,status);row.append(line);
   const hint=document.createElement('p');hint.className='inspect-caption';hint.textContent=m.error||m.description||m.id;row.append(hint);
   if(!m.active&&!m.error){const button=document.createElement('button');button.className='action strong';button.textContent='ENABLE MODULE';
     button.onclick=()=>{if(!confirm('Trust and execute local Python code in '+m.name+'? Only enable modules you trust.'))return;
       send({type:'module_enable',id:m.id});};row.append(button);}
   panel.append(row);
 }
}
// Manager shares the slide-in panel class with AUDIO SETUP: its .visible class
// controls the transform. Merely setting display:block leaves it off-screen.
function setModuleManagerVisible(visible){
 const panel=$('#moduleManager');
 panel.classList.toggle('visible',!!visible);
 panel.setAttribute('aria-hidden',String(!visible));
 $('#moduleScanBtn').setAttribute('aria-expanded',String(!!visible));
}
$('#moduleScanBtn').onclick=()=>{setModuleManagerVisible(true);send({type:'modules_scan'});};
$('#closeModuleManager').onclick=()=>setModuleManagerVisible(false);
$('#rescanModules').onclick=()=>send({type:'modules_scan'});
function connect(){
 const url=`${location.protocol==='https:'?'wss':'ws'}://${location.host}/ws`;
 ws=new WebSocket(url);
 ws.onopen=()=>{connected=true;badge.textContent='HOST / CONNECTED';send({type:'devices'});};
 ws.onclose=()=>{connected=false;hostReady=false;badge.textContent='HOST / DISCONNECTED';badge.classList.add('offline');setTimeout(connect,1400);};
 ws.onerror=()=>{};
 ws.onmessage=async e=>{
  let msg;try{msg=JSON.parse(e.data)}catch{return;}
  if(msg.type==='hello'){
    schema=msg.schema;
    presetCatalog=schema.presets||[];
    try{await loadModuleAssets(schema.module_assets||{});}catch(err){notice('Some module controls could not load: '+err.message);console.warn('Wirescape module UI:',err);}
    for(const [type,d] of Object.entries(schema.plugin_definitions||{}))window.__wirescapeTest.registerDefinition(type,d);
    renderModuleCatalog(schema.installed_modules||[]);
    // Plugins may not be defined when the static HTML's initial localStorage load runs.
    const deferred=window.__wirescapeDeferredPatch;
    if(deferred){
      const missing=[...new Set(deferred.modules.filter(m=>!['chaos','modulator','interface'].includes(m.type)&&!schema.plugin_definitions?.[m.type]).map(m=>m.type))];
      if(missing.length)notice('Saved patch needs inactive modules: '+missing.join(', ')+'. Open MODULES ↻ to enable them.');
      else try{window.__wirescapeTest.load(deferred);delete window.__wirescapeDeferredPatch;}catch(err){notice('Could not restore plugin patch: '+err.message);}
    }
    refreshPresets();
    hostReady=msg.surge_ready;
    badge.textContent=hostReady?'SURGE / READY':'SURGE / MISSING';badge.classList.toggle('offline',!hostReady);
    badge.title=msg.surge_error||'Compiled Surge module found';
    if(!hostReady)notice('Surge XT is not linked. Editor works; synthesizer audio is silent. See README.');
    lastTopology='';patchChanged(data(),true);setStatus(msg.status);if(inspect.classList.contains('visible'))renderDeep();
  }else if(msg.type==='modules_catalog'){
    renderModuleCatalog(msg.modules||[]);
  }else if(msg.type==='modules_changed'){
    schema=msg.schema;
    try{await loadModuleAssets(schema.module_assets||{});}catch(err){notice('Some module controls could not load: '+err.message);console.warn('Wirescape module UI:',err);}
    for(const [type,d] of Object.entries(schema.plugin_definitions||{}))window.__wirescapeTest.registerDefinition(type,d);
    renderModuleCatalog(msg.modules||[]);
    const deferred=window.__wirescapeDeferredPatch;
    if(deferred&&!deferred.modules?.some(m=>!schema.plugin_definitions[m.type]&&!['chaos','modulator','interface'].includes(m.type))){
      window.__wirescapeTest.load(deferred);delete window.__wirescapeDeferredPatch;
    }
    patchChanged(data(),true);notice('Module enabled. You can add it from MODULES.');
  }else if(msg.type==='devices'){
    const box=$('#deviceSelect');box.replaceChildren();
    const opt=document.createElement('option');opt.value='';opt.textContent='System default output';box.append(opt);
    for(const d of msg.devices||[]){if(!d.output)continue;const el=document.createElement('option');el.value=d.id;el.textContent=`${d.name} (${d.output} ch)`;box.append(el);}
  }else if(msg.type==='meters')setStatus(msg.state);
  else if(msg.type==='audio')setStatus(msg.state);
  else if(msg.type==='error'){notice(msg.message);console.warn('Wirescape host:',msg.message);}
  else if(msg.type==='preset_loaded'){
   if(window.__wirescapeTest.applyChaosPreset(msg.id,msg.preset)){
    patchChanged(data(),true);
    if(selected?.id===msg.id&&inspect.classList.contains('visible'))renderDeep();
    notice('CHAOS preset loaded · all controls synchronized.');
   }
  }else if(msg.type==='preset_saved'){
    presetCatalog=msg.presets||presetCatalog;if(schema)schema.presets=presetCatalog;refreshPresets();
    const sel=document.querySelector(`[data-presets-for="${CSS.escape(msg.id)}"]`);
    if(sel)sel.value=msg.entry.id;
    notice('Preset saved: '+msg.entry.name);
  }

 };
}
function moduleSelected(){if(selected?.type!=='module')return null;return data()?.modules.find(m=>m.id===selected.id);}
function patchModule(id){return data()?.modules.find(m=>m.id===id);}
function modeLabel(m){return (schema?.modes||{})[m.type]?.[m.mode]||'';}
function detailTabs(m){
 const bar=document.createElement('nav');bar.className='deep-tabs';
 const tabs=window.WirescapeModuleUI?.[m.type]?.tabs||
   (m.type==='chaos'?['PANEL','MACROS','RAW A','RAW B','ROUTING']:m.type==='modulator'?['CONTROLS','PORTS']:['CONTROLS','PORTS']);
 const active=inspect.dataset.tab&&tabs.includes(inspect.dataset.tab)?inspect.dataset.tab:tabs[0];inspect.dataset.tab=active;
 for(const name of tabs){const b=document.createElement('button');b.textContent=name;b.className=name===active?'on':'';b.onclick=()=>{inspect.dataset.tab=name;renderDeep();};bar.append(b);}
 return bar;
}
function header(text,detail=''){const h=document.createElement('div');h.className='deep-heading';h.textContent=text;if(detail){const small=document.createElement('small');small.textContent=detail;h.append(small);}return h;}
function slider(opts){
 const row=document.createElement('div');row.className='deep-param';
 const labels=document.createElement('div');labels.className='deep-param-meta';
 const name=document.createElement('label');name.textContent=opts.name;name.title=opts.hint||'';row.dataset.help=opts.hint||'';
 const val=document.createElement('output');val.textContent=opts.label||String(opts.value);labels.append(name,val);
 const input=document.createElement('input');input.type='range';input.min=opts.min??0;input.max=opts.max??100;input.step=opts.step??1;input.value=opts.value;
 input.setAttribute('aria-label',opts.name);
 input.oninput=()=>{val.textContent=Number(input.value).toFixed((opts.step??1)<1?2:0);opts.set(Number(input.value));if(opts.override)row.classList.add('overridden');};
 row.append(labels,input);
 if(opts.override){row.classList.add('overridden');const clear=document.createElement('button');clear.className='deep-reset';clear.textContent='CLEAR OVERRIDE';clear.onclick=()=>{opts.clear?.();renderDeep();};row.append(clear);}
 return row;
}
// Discrete controls must not masquerade as continuous normalized 0..1 sliders.
// We use the actual Surge snapshot min/max/type, with optional scanned label lists.
function selectRow(opts){
 const row=document.createElement('div');row.className='deep-param';row.dataset.help=opts.hint||'';
 const labels=document.createElement('div');labels.className='deep-param-meta';
 const name=document.createElement('label');name.textContent=opts.name;
 const val=document.createElement('output');val.textContent=opts.override?'OVERRIDE':'NO OVERRIDE';
 labels.append(name,val);row.append(labels);
 const select=document.createElement('select');select.className='deep-select';select.setAttribute('aria-label',opts.name);
 if(opts.options.length>150){row.append(document.createTextNode('Too many discrete choices'));return row;}
 for(const [value,label] of opts.options)select.append(new Option(label,String(value)));
 select.value=String(opts.value);
 select.onchange=()=>{val.textContent=select.selectedOptions[0]?.textContent||'';opts.set(Number(select.value));row.classList.add('overridden');};
 row.append(select);
 if(opts.hint){const caption=document.createElement('div');caption.className='deep-choice-hint';caption.textContent=opts.hint;row.append(caption);}
 if(opts.override){row.classList.add('overridden');const clear=document.createElement('button');clear.className='deep-reset';clear.textContent='CLEAR OVERRIDE';clear.onclick=()=>{opts.clear();renderDeep();};row.append(clear);}
 return row;
}
function rawSpec(key){return schema?.raw_metadata?.[key]||schema?.raw_metadata?.['S1:'+key]||null;}
function rawDefaultNorm(meta){if(!meta||!Number.isFinite(meta.max-meta.min)||meta.max===meta.min)return .5;return Math.min(1,Math.max(0,(meta.def-meta.min)/(meta.max-meta.min)));}
function rawEditor(m,voice,key,label){
 const rkey=`${voice}|${key}`,existing=m.raw?.[rkey],override=existing!==undefined;
 const meta=rawSpec(key),isInt=meta?.type==='int',isBool=meta?.type==='bool';
 const range=meta?(meta.max-meta.min):0;
 const current=override?existing:rawDefaultNorm(meta);
 const hint=meta?`Surge ${meta.type}. Range ${meta.min}–${meta.max}. Snapshot default: ${meta.display}. ${override?'This is an override.':'Unedited parameter follows the engine; the starting position is a reference, not a live meter.'}`:'Normalized Surge or modulation-matrix depth. Centre is neutral where applicable.';
 const set=v=>{const node=patchModule(m.id);if(!node)return;window.__wirescapeTest.setRaw(node.id,rkey,v);send({type:'raw',id:node.id,voice,name:key,value:v});window.__wirescapeTest.save();};
 const clear=()=>{const node=patchModule(m.id);if(!node)return;window.__wirescapeTest.setRaw(node.id,rkey,0,true);send({type:'raw',id:node.id,voice,name:key,clear:true});window.__wirescapeTest.save();};
 if((isBool||isInt)&&range>=0&&range<=96&&Number.isInteger(range)){
  const labels=schema?.enum_labels?.[key]||schema?.enum_labels?.['S1:'+key]||{};
  const options=[];
  for(let i=meta.min;i<=meta.max;i++){
   const lname=labels[i]||(i===meta.def?meta.display:null);
   const family=isBool?(i?'On':'Off'):(/type|subtype|configuration|mode|routing/i.test(key)?'Type ':'Value ')+i;
   options.push([(i-meta.min)/Math.max(range,1),lname?`${i} · ${lname}`:`${i} · ${family}`]);
  }
  const picked=Math.max(meta.min,Math.min(meta.max,Math.round(meta.min+current*range)));
  const optionRows=options.map(([v,text])=>[Math.round((meta.min+v*range)),text]);
  return selectRow({name:label,hint,options:optionRows,value:picked,override,
   set:n=>set((n-meta.min)/Math.max(range,1)),clear});
 }
 return slider({name:label,min:0,max:1,step:0.001,value:current,label:override?Number(current).toFixed(3):'NO OVERRIDE',override,hint,set,clear});
}
function macroRow(m,key,spec){
 const existing=m.advanced?.[key],override=existing!==undefined;
 const val=override?existing:spec.default;
 const hint=schema?.macro_descriptions?.[key]||`Original CHAOS macro · range ${spec.min} to ${spec.max}.`;
 const setter=v=>{const node=patchModule(m.id);if(!node)return;window.__wirescapeTest.setAdvanced(node.id,key,v);send({type:'advanced',id:node.id,name:key,value:v});window.__wirescapeTest.save();};
 const clearer=()=>{const node=patchModule(m.id);if(!node)return;window.__wirescapeTest.setAdvanced(node.id,key,0,true);send({type:'advanced',id:node.id,name:key,clear:true});window.__wirescapeTest.save();};
 const choices={
  DRIVE_TYPE:['Soft','Hard','Asymmetric','Digital','Double Fold','West Coast','Heavy Fuzz','OJD'],
  ROUTE_MODE:['Parallel','Ring / AM','Delay Bend'],QUARTER:['Semitone clusters','Quarter-tone clusters'],
  A_MUTE:['On','Muted'],B_MUTE:['On','Muted'],
  ...Object.fromEntries([1,2,3,4].flatMap(i=>[
    [`MOD${i}_SRC`,['Off','Self LFO','Sample & hold','Smooth random','Output envelope']],
    [`MOD${i}_DST`,['Off','BREAK','DETUNE','CUTOFF','ROUGH DEPTH','LOOP SPEED','LOOP FEEDBACK','TAPE','SPACE MIX','ROUTE DEPTH','BODY MIX','BODY FEEDBACK']]
  ]))
 };
 if(choices[key])return selectRow({name:key.replaceAll('_',' '),value:val,hint,override,clear:clearer,set:setter,
   options:choices[key].map((txt,i)=>[i,txt])});
 if(spec.integer && spec.max-spec.min<=20){const options=[];for(let i=spec.min;i<=spec.max;i++)options.push([i,String(i)]);
  return selectRow({name:key.replaceAll('_',' '),value:val,options,hint,override,set:setter,clear:clearer});}
 return slider({name:key.replaceAll('_',' '),min:spec.min,max:spec.max,step:spec.integer?1:Math.max(.001,(spec.max-spec.min)/1000),
  value:val,override,hint,set:setter,clear:clearer});
}
function renderDeep(){
 deepBody.replaceChildren();const m=moduleSelected();
 if(!m){deepBody.append(header('SELECT A MODULE','Click a node in the patch to edit its details.'));return;}
 deepName.textContent=`${m.type.toUpperCase()} / ${m.id}`;
 deepBody.append(detailTabs(m));
 const tab=inspect.dataset.tab;
 const pluginUI=window.WirescapeModuleUI?.[m.type];
 if(pluginUI?.renderEditor){
   pluginUI.renderEditor(deepBody,m,tab,
      ()=>{window.__wirescapeTest.save();send({type:'module_state',id:m.id,state:m.module_state});},
      (key,value)=>{window.__wirescapeTest.updateKnob(m.id,key,value);control(m.id,key,value);window.__wirescapeTest.save();},
      ()=>renderDeep());
   return;
 }
 if(m.type==='chaos' && tab==='MACROS'){
  deepBody.append(header('ADVANCED MACROS','All original CHAOS macro parameters. Edited values override front-panel bundles until cleared.'));
  for(const [group,names] of Object.entries(schema?.groups||{})){
   const details=document.createElement('details');details.className='param-group';details.open=group==='PERFORMANCE';
   const summary=document.createElement('summary');summary.textContent=group;details.append(summary);
   for(const k of names){const spec=schema.macros[k];if(!spec)continue;const existing=m.advanced?.[k];const isOverride=existing!==undefined;
    const row=macroRow(m,k,spec);
    details.append(row);
   }deepBody.append(details);
  }
 }else if(m.type==='chaos'&&tab.startsWith('RAW')){
   const voice=tab.slice(-1);deepBody.append(header(`RAW SURGE / VOICE ${voice}`,'Normalized controls, including all three oscillators, filter envelopes, LFOs, and modulation assignments. Explicit raw edits override macro targets.'));
   // The first groups are intentionally curated. The expandable full index below
   // exposes every parameter described by CHAOS's Surge introspection snapshot.
   const all=document.createElement('details');all.className='param-group full-raw';
   const summary=document.createElement('summary');summary.textContent='FULL ENGINE PARAMETER INDEX / '+Object.values(schema?.full_raw_groups||{}).reduce((n,rows)=>n+rows.length,0)+' CONTROLS';all.append(summary);
   const fullContents=document.createElement('div');all.append(fullContents);
   all.addEventListener('toggle',()=>{if(!all.open||all.dataset.rendered)return;all.dataset.rendered='1';
     const hint=document.createElement('p');hint.className='deep-copy';hint.textContent='Each parameter is addressed by its original Surge name and scene. Values are normalized 0–1; unedited controls retain the engine’s current value. Expand only the section you need.';fullContents.append(hint);
     for(const [group,rows] of Object.entries(schema?.full_raw_groups||{})){
       const section=document.createElement('details');section.className='param-group';const title=document.createElement('summary');title.textContent=`${group} / ${rows.length}`;section.append(title);
       section.addEventListener('toggle',()=>{if(!section.open||section.dataset.rendered)return;section.dataset.rendered='1';
         for(const [key,label] of rows){section.append(rawEditor(moduleSelected(),voice,key,label));
         }
       });fullContents.append(section);
     }
   });deepBody.append(all);
   for(const [group,names] of schema?.raw_groups||[]){const details=document.createElement('details');details.className='param-group';details.open=group==='OSC 1';const summary=document.createElement('summary');summary.textContent=group;details.append(summary);
    for(const key of names){details.append(rawEditor(moduleSelected(),voice,key,key));
    }deepBody.append(details);
   }
 }else if(m.type==='chaos'&&tab==='ROUTING'){
   deepBody.append(header('SIGNAL ARCHITECTURE','Advanced routing lives here; changes are applied to the actual CHAOS engine.'));
   const info=document.createElement('p');info.className='deep-copy';info.textContent='Two complete Surge XT synths → generator interaction → resonant body → loop + FX. The front-panel MORPH is an equal-power A/B crossfade; FOLD uses the existing BREAK waveshaper. All four internal modulation slots and direct bus controls are under MACROS.';deepBody.append(info);
   const buttons=document.createElement('button');buttons.className='action strong';buttons.textContent='OPEN ROUTING PARAMETERS';buttons.onclick=()=>{inspect.dataset.tab='MACROS';renderDeep();};deepBody.append(buttons);
 }else{
   deepBody.append(header('EXPOSED PARAMETERS',m.type==='chaos'?'30 performance controls. Detailed macro and Surge edit pages are in adjacent tabs.':'Parameters send live changes to the DSP host.'));
   const controls=(m.type==='chaos'?Object.keys(schema?.front_controls||{}):
     m.type==='modulator'?['RATE','SHAPE','DEPTH','DRIFT','RANDOM']:
     m.type==='interface'?['OUTPUT LEVEL','INPUT GAIN']:
     (schema?.plugin_definitions?.[m.type]?.controls||[]).map(pair=>pair[0]));
   for(const key of controls){const cur=m.values?.[key]??(m.type==='chaos' ? (schema?.front_controls||{})[key]:m.type==='interface'?{'OUTPUT LEVEL':71,'INPUT GAIN':36}[key]:
         m.type==='modulator'?{RATE:44,SHAPE:53,DEPTH:67,DRIFT:29,RANDOM:34}[key]:
         Object.fromEntries(schema?.plugin_definitions?.[m.type]?.controls||[])[key]);
    deepBody.append(slider({name:key,min:0,max:100,value:cur,hint:schema?.front_descriptions?.[key]||'',
      set(v){const mm=moduleSelected();if(!mm)return;window.__wirescapeTest.updateKnob(mm.id,key,v);control(mm.id,key,v);window.__wirescapeTest.save();}}));
   }
   if(m.type==='chaos'){
    deepBody.append(header('CHARACTER SCENE','These are parameter/routing scenes, not distinct synth engines.'));
    const p=document.createElement('p');p.className='deep-copy';p.textContent='CLUSTER: poly-note clusters; FM: FM ratio and modulated-delay bus interaction; NOISE: noise generator plus ring routing.';deepBody.append(p);
    const sound=document.createElement('div');sound.className='deep-presets';sound.append(header('CHAOS PRESETS','Factory or user presets can be loaded from the module front panel. User presets preserve deep/raw edits, mode and front settings.'));
    for(const preset of schema?.presets||[]){const b=document.createElement('button');b.textContent=(preset.kind==='factory'?'FACTORY / ':'USER / ')+preset.name;b.onclick=()=>loadPreset(m.id,preset.id);sound.append(b);}deepBody.append(sound);
   }
   if(m.type!=='chaos'&&tab==='PORTS'){
    deepBody.append(header('SIGNALS','Patch outputs to matching inputs on other modules.'));
    const list=document.createElement('p');list.className='deep-copy';list.textContent=m.type==='modulator'?'MOD A, MOD B are bipolar control-rate signals. GATE OUT emits block-rate pulses. CLOCK and RESET can drive the modulation phase.':'AUDIO IN L/R receive mono buffers summed at the interface output. EXT IN L/R carry actual hardware input when duplex mode is enabled.';deepBody.append(list);
   }
 }
}
function showDeep(){inspect.classList.add('visible');renderDeep();}
function openEngine(id){if(id){window.__wirescapeTest.choose({type:'module',id});inspect.dataset.tab='MACROS';}showDeep();}
function openModuleEditor(id,index){
 window.__wirescapeTest.choose({type:'module',id});
 const m=patchModule(id),ui=window.WirescapeModuleUI?.[m?.type];
 if(ui?.selectedStep&&Number.isInteger(index))ui.selectedStep(id,index);
 inspect.dataset.tab=ui?.tabs?.[0]||'CONTROLS';showDeep();
}
function closeDeep(){inspect.classList.remove('visible');}
inspectBtn.onclick=()=>inspect.classList.contains('visible')?closeDeep():showDeep();
$('#deepLaunch').onclick=showDeep;$('#closeDeep').onclick=closeDeep;
deviceBtn.onclick=()=>{devicePanel.classList.toggle('visible');send({type:'devices'});};
$('#closeDevices').onclick=()=>devicePanel.classList.remove('visible');
$('#audioToggle').onclick=()=>{
 if(audioRunning)send({type:'audio_stop'});
 else send({type:'audio_start',device:$('#deviceSelect').value||null,input:$('#inputEnable').checked});
};
$('#panicBtn').onclick=()=>{send({type:'panic'});activeNotes.clear();};
const audition=$('#auditionBtn');
function noteStart(note){
 if(activeNotes.has(note))return;
 const ids=new Set(routingTargets());if(!ids.size)return;
 activeNotes.set(note,ids);
 for(const id of ids)send({type:'note',id,note,on:true});
}
function noteStop(note){
 const ids=activeNotes.get(note);if(!ids)return;activeNotes.delete(note);
 for(const id of ids){if(patchModule(id))send({type:'note',id,note,on:false});}
}
let baseNote=60;$('#noteSelect').onchange=e=>{baseNote=Number(e.target.value)};
let heldAudition=null;
audition.addEventListener('pointerdown',e=>{e.preventDefault();heldAudition=baseNote;noteStart(heldAudition);audition.setPointerCapture(e.pointerId);});
for(const ev of ['pointerup','pointercancel','lostpointercapture'])audition.addEventListener(ev,()=>{if(heldAudition!==null){noteStop(heldAudition);heldAudition=null;}});
const notes={a:0,w:1,s:2,e:3,d:4,f:5,t:6,g:7,y:8,h:9,u:10,j:11,k:12};
function isTypingTarget(el){return !!el?.closest('[contenteditable="true"],textarea,input:not([type="range"]):not([type="checkbox"])');}
document.addEventListener('keydown',e=>{
 if(e.repeat||e.altKey||e.metaKey||e.ctrlKey||isTypingTarget(e.target))return;
 const offset=notes[e.key.toLowerCase()];if(offset!==undefined){e.preventDefault();noteStart(baseNote+offset);}
});
document.addEventListener('keyup',e=>{const offset=notes[e.key.toLowerCase()];if(offset!==undefined)noteStop(baseNote+offset);});
window.addEventListener('blur',()=>{for(const n of [...activeNotes.keys()])noteStop(n);});
// Minimal, accessible parameter explainer. Audio/Surge descriptions are tooltips,
// not an extra giant help panel covering the synthesizer.
const help=$('#paramHelp');
function showHelp(e){const el=e.target.closest?.('[data-help]');if(!el||!el.dataset.help)return;
 help.textContent=el.dataset.help;help.hidden=false;
 const rect=el.getBoundingClientRect();const w=Math.min(285,innerWidth-28);
 help.style.maxWidth=w+'px';help.style.left=Math.min(innerWidth-w-14,Math.max(14,rect.left))+'px';
 help.style.top=Math.max(12,Math.min(innerHeight-100,rect.bottom+8))+'px';
}
function hideHelp(e){if(!e.relatedTarget?.closest?.('[data-help]'))help.hidden=true;}
document.addEventListener('mouseover',showHelp);
document.addEventListener('focusin',showHelp);
document.addEventListener('mouseout',hideHelp);
document.addEventListener('focusout',hideHelp);
window.addEventListener('wirescape:state',e=>patchChanged(e.detail));
if(location.protocol==='file:'){badge.textContent='PREVIEW / OFFLINE';badge.classList.add('offline');notice('Open with the Python Wirescape host to use audio and deep controls.');}else connect();
})();
