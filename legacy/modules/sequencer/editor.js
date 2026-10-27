/* WIRESCAPE / CHORD SEQUENCER. UI only. The audio callback runs in Python. */
(() => {
'use strict';
const ROOTS=['C','C♯','D','D♯','E','F','F♯','G','G♯','A','A♯','B'];
const QUALITIES=['maj','min','dim','aug','sus2','sus4','5','6','m6','7','maj7','m7','mMaj7','dim7','m7b5','add9','madd9','9','maj9','m9'];
const selected=new Map();
function el(tag,cls,text){const e=document.createElement(tag);if(cls)e.className=cls;if(text!==undefined)e.textContent=text;return e;}
function label(step){return ROOTS[step.root]+step.quality;}
function selectedStep(id,step){if(Number.isInteger(step)&&step>=0&&step<16)selected.set(id,step);}
function indexFor(m){return selected.get(m.id)??0;}
function paint(m){
 const host=document.querySelector(`.module[data-id="${CSS.escape(m.id)}"] .seq-pads`);
 if(!host)return;
 for(const pad of host.querySelectorAll('button')){
   const idx=Number(pad.dataset.index),step=m.module_state?.steps[idx];if(!step)continue;
   pad.classList.toggle('active',step.enabled&&idx<m.module_state.length);
   pad.classList.toggle('selected',idx===indexFor(m));
   pad.classList.toggle('outside',idx>=m.module_state.length);
   pad.querySelector('.seq-label').textContent=step.enabled?label(step):'—';
   pad.querySelector('.seq-number').textContent=String(idx+1).padStart(2,'0');
   pad.querySelector('.seq-marker').textContent=step.trig?'●':'·';
 }
}
function face(m,core,open){
 const topline=el('div','seq-topline');
 topline.append(el('span','', 'HARMONIC EVENTS / 16 STEPS'));
 const edit=el('button','seq-action','EDIT SEQUENCE ↗');
 edit.addEventListener('pointerdown',e=>e.stopPropagation());
 edit.onclick=e=>{e.stopPropagation();open(indexFor(m));};topline.append(edit);core.append(topline);
 const pads=el('div','seq-pads');
 for(let i=0;i<16;i++){
   const pad=el('button','seq-pad');pad.type='button';pad.dataset.index=i;
   pad.append(el('span','seq-number'),el('strong','seq-label'),el('span','seq-marker'));
   pad.title=`Edit chord and modulation at step ${i+1}`;
   pad.addEventListener('pointerdown',e=>e.stopPropagation());
   pad.onclick=e=>{e.stopPropagation();selected.set(m.id,i);paint(m);open(i);};pads.append(pad);
 }
 core.append(pads);
 const help=el('div','seq-footnote','SELECT STEP TO EDIT CHORD · TRIGGER · FILTER CV');core.append(help);
 requestAnimationFrame(()=>paint(m));
}
function field(title,value,options,change){
 const row=el('label','seq-field');row.append(el('span','',title));
 const s=el('select');s.setAttribute('aria-label',title);
 for(const [key,text] of options){const opt=el('option','',text);opt.value=String(key);s.append(opt);}
 s.value=String(value);s.onchange=()=>change(s.value);row.append(s);return row;
}
function toggle(title,value,change){
 const row=el('label','seq-toggle');const input=el('input');input.type='checkbox';input.checked=!!value;
 input.onchange=()=>change(input.checked);row.append(input,el('span','',title));return row;
}
function range(title,value,min,max,step,change,display){
 const row=el('label','seq-range');const line=el('span','seq-range-head');
 line.append(el('span','',title));const output=el('output','',display?display(value):String(value));line.append(output);row.append(line);
 const input=el('input');input.type='range';input.min=min;input.max=max;input.step=step;input.value=value;
 input.oninput=()=>{const v=Number(input.value);output.textContent=display?display(v):String(v);change(v);};row.append(input);return row;
}
function numOptions(a,b,transform=x=>x){return Array.from({length:b-a+1},(_,i)=>[i+a,transform(i+a)]);}
function renderEditor(container,m,tab,changed,changedControl,rerender){
 const pattern=m.module_state;if(!pattern){container.append(el('p','deep-copy','Sequencer state unavailable. Reinsert this module.'));return;}
 const persist=()=>{paint(m);changed();};
 const heading=(name,desc)=>{container.append(el('div','deep-heading',name),el('p','deep-copy',desc));};
 if(tab==='STEPS'){
   heading('HARMONIC STEP EDITOR','One step holds a full chord, gate time, strum, step CV and envelope trigger. 16th-note grid at the module BPM. Disable individual steps for rests.');
   const row=el('div','seq-step-selector');
   for(let i=0;i<16;i++){
     const b=el('button','seq-step-tab'+(indexFor(m)===i?' on':''),String(i+1).padStart(2,'0'));
     b.disabled=i>=pattern.length;b.onclick=()=>{selected.set(m.id,i);paint(m);rerender();};row.append(b);
   }container.append(row);
   const idx=Math.min(pattern.length-1,indexFor(m));selected.set(m.id,idx);
   const step=pattern.steps[idx];
   container.append(el('div','deep-heading',`STEP ${String(idx+1).padStart(2,'0')} / ${label(step)}`));
   const picks=el('div','seq-form-grid');
   picks.append(
     field('ROOT',step.root,ROOTS.map((r,i)=>[i,r]),v=>{step.root=Number(v);persist();rerender();}),
     field('CHORD',step.quality,QUALITIES.map(q=>[q,q]),v=>{step.quality=v;persist();rerender();}),
     field('OCTAVE',step.octave,numOptions(1,6),v=>{step.octave=Number(v);persist();}),
     field('INVERSION',step.inversion,numOptions(0,4),v=>{step.inversion=Number(v);persist();})
   );container.append(picks);
   const flags=el('div','seq-flags');
   flags.append(
     toggle('PLAY CHORD',step.enabled,v=>{step.enabled=v;persist();}),
     toggle('TRIGGER ENVELOPE',step.trig,v=>{step.trig=v;persist();}),
     toggle('OPEN VOICING',step.spread,v=>{step.spread=v;persist();})
   );container.append(flags);
   container.append(field('CHORD DURATION / 16TH-NOTE STEPS',step.duration_steps,numOptions(1,16),v=>{step.duration_steps=Number(v);persist();}));
   container.append(field('PLAY EVERY NTH LOOP',step.every,numOptions(1,8),v=>{step.every=Number(v);persist();}));
   container.append(range('STRUM',step.strum_ms,0,200,1,v=>{step.strum_ms=v;persist();},v=>v+' ms'));
   container.append(range('NOTE LENGTH',step.gate,0.05,1,0.01,v=>{step.gate=v;persist();},v=>Math.round(v*100)+'%'));
   container.append(range('PROBABILITY',step.probability,0,1,0.01,v=>{step.probability=v;persist();},v=>Math.round(v*100)+'%'));
   container.append(range('STEP CV (BIPOLAR)',step.mod,-1,1,0.01,v=>{step.mod=v;persist();},v=>v.toFixed(2)));
   const copy=el('button','seq-action seq-copy','COPY STEP TO NEXT');copy.onclick=()=>{
     const to=(idx+1)%16;pattern.steps[to]=JSON.parse(JSON.stringify(step));selected.set(m.id,to);persist();rerender();
   };container.append(copy);
 } else if(tab==='MOD LANE'){
   heading('INDEPENDENT MODULATION','16 programmable bipolar values advance at their own rate on INTERNAL clock. On EXTERNAL clock, division skips ahead by N values on each clock edge. The MOD output blends this lane and each chord step’s CV, then applies DEPTH. Patch MOD → CHAOS CUTOFF, PITCH, MORPH, or CHAOS.');
   container.append(field('ACTIVE MOD STEPS',pattern.mod_length,numOptions(1,16),v=>{pattern.mod_length=Number(v);persist();rerender();}));
   container.append(field('MOD UPDATES PER CHORD STEP',pattern.mod_division,numOptions(1,8),v=>{pattern.mod_division=Number(v);persist();rerender();}));
   const rack=el('div','seq-mod-rack');
   for(let i=0;i<16;i++){
     const lane=el('label','seq-mod-step'+(i>=pattern.mod_length?' outside':''));lane.append(el('span','',String(i+1).padStart(2,'0')));
     const inp=el('input');inp.type='range';inp.min=-1;inp.max=1;inp.step=0.01;inp.value=pattern.modulation[i];
     const out=el('output','',Number(pattern.modulation[i]).toFixed(2));inp.oninput=()=>{pattern.modulation[i]=Number(inp.value);out.textContent=Number(inp.value).toFixed(2);persist();};
     lane.append(inp,out);rack.append(lane);
   }container.append(rack);
 } else if(tab==='SETTINGS'){
   heading('TRANSPORT / PERFORMANCE','The internal transport runs 16th-note steps. External clock advances one step per rising edge. Reset restarts at step 1. Toggle RUN to pause and release held notes.');
   container.append(field('ACTIVE CHORD STEPS',pattern.length,numOptions(1,16),v=>{pattern.length=Number(v);persist();rerender();}));
   const controls={RUN:[0,100,100,'OFF / ON'],BPM:[0,100,1,'40–240 BPM'],SWING:[0,100,1,'0–49%'],DEPTH:[0,100,1,'CV DEPTH'],DECAY:[0,100,1,'25–2025 ms']};
   for(const [key,[min,max,unit,hint]] of Object.entries(controls)){
     const value=m.values[key]??({RUN:100,BPM:50,SWING:0,DEPTH:100,DECAY:35})[key];
     container.append(range(key+' / '+hint,value,min,max,unit,v=>{m.values[key]=v;changedControl(key,v);},v=>key==='BPM'?(40+2*v).toFixed(0)+' BPM':key==='RUN'?(v>=50?'PLAY':'STOP'):Math.round(v)+'%'));
   }
 }else if(tab==='PORTS'){
   heading('PATCH CONNECTIONS','NOTES → CHAOS NOTES IN (polyphonic chords). TRIG → gate/clock inputs for modulation. GATE → sustained gate destinations. MOD → bipolar CV input (including CHAOS CUTOFF). ENV → envelope CV. CLOCK IN accepts gate/clock pulses in EXTERNAL mode; RESET restarts the sequence.');
   container.append(el('p','deep-copy','Timing resolution is one audio callback (512 frames / ~10.7 ms at 48 kHz); this is not a sample-accurate MIDI sequencer. Note velocities, polyphonic expression, true per-note timing and MPE are not supported by the current CHAOS host contract.'));
 }
}
window.WirescapeModuleUI??={};
window.WirescapeModuleUI.sequencer={tabs:['STEPS','MOD LANE','SETTINGS','PORTS'],face,renderEditor,selectedStep,paint};
})();
