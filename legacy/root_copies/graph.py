"""Audio graph runner. DSP topology compiled from a typed, cycle-free patch document.

Host semantics:
* exactly one PortAudio output, irrespective of number of sound-producing modules
* each CHAOS instance owns its own dual-Surge engine and polyphony state
* audio L/R ports are mono float32 signal blocks; controls are bipolar scalars
* all inter-module data is routed inside the callback, NEVER through the browser
* editor-to-audio mutations travel through a SimpleQueue, drained at block start
* module constructors and graph validation run on the host control thread

This is a Python reference host, not a hard realtime engine: Python allocation,
GIL, and legacy Surge per-block setters must be profiled before Pi deployment.
"""
import math
import queue
import time
from dataclasses import dataclass
from collections import defaultdict
import numpy as np
from .schema import DEFINITIONS, validate_patch, normalize_control, PatchError
from .modulation import Modulator
from .plugins import AUDIO_PROCESSORS
from .mapping import chaos_macros, CHAOS_MOD_DEST, FRONT_OWNS

SR=48000
BLOCK=512

from functools import lru_cache
@lru_cache(maxsize=1)
def full_raw_keys():
    import json
    from pathlib import Path
    path=Path(__file__).resolve().parents[1]/'modules'/'chaos'/'params.json'
    return frozenset(f"S{p['scene']}:{p['name']}" for p in json.loads(path.read_text(encoding='utf8')))

def send_raw(synth, voice, key, norm, clear=False):
    if key in full_raw_keys():
        scene=int(key[1]);name=key[3:]
        if clear:synth.q.put(('surge_raw_clear',voice,(scene,name)))
        else:synth.q.put(('surge_raw',voice,(scene,name,norm)))
    else:
        if clear:synth.q.put(('raw_clear',voice,key))
        else:synth.q.put(('raw',voice,(key,norm)))


class SilentSynth:
    """No compiled surgepy available. Silent only; never pretend it is CHAOS."""
    def __init__(self, sr=SR):
        self.state={}
        self.raw={"A":{},"B":{}}
        self.labels={"A":{},"B":{}}
        self.q=queue.SimpleQueue()
    def set_many(self, vals):self.state.update(vals)
    def render(self,n,input_block=None):return np.zeros((2,n),np.float32)
    def note_on(self,n):pass
    def note_off(self,n):pass
    def panic(self):pass

@dataclass(frozen=True)
class Link:
    src: str
    out: str
    dst: str
    inport: str
    kind: str

class NodeRuntime:
    def __init__(self,node,engine_factory,sr):
        self.id=node['id']
        self.type=node['type']
        spec=DEFINITIONS[self.type]
        self.controls=dict(spec['controls'])
        self.controls.update({k:normalize_control(self.type,k,v) for k,v in node.get('values',{}).items() if k in self.controls})
        self.mode=max(0,min(len(spec['modes'])-1,int(node.get('mode',0)))) if spec['modes'] else 0
        self.advanced={}
        self.raw_overrides={}
        self.raw_pending=[]
        self.last_macros={}
        self.last_noise=None
        self.mod=Modulator(sr,seed=sum(map(ord,self.id))) if self.type=='modulator' else None
        self.plugin=AUDIO_PROCESSORS[self.type](sr) if self.type in AUDIO_PROCESSORS else None
        self.synth=engine_factory(sr) if self.type=='chaos' else None
        self.output_gain=0.0
        self.last_rms=0.0
        self.monitor=bool(node.get('monitor',False))
        self.gate_high=False
        self.note_owners={}  # note -> {('sequence',source),('gate',...),('keyboard',...)}

    def change(self,key,value):
        self.controls[key]=normalize_control(self.type,key,value)

class AudioGraph:
    def __init__(self,sr=SR,engine_factory=None):
        self.sr=sr
        self.block=BLOCK
        self.engine_factory=engine_factory or (lambda sr: SilentSynth(sr))
        self.queue=queue.SimpleQueue()
        self.nodes={}
        self.order=()
        self.links=()
        self.report={'peak_l':0.0,'peak_r':0.0,'rms_l':0.0,'rms_r':0.0,'render_ms':0.0,'xruns':0,'modules':0}
        self._last_report=0.0

    def load_patch(self,patch):
        """Control thread. Prepare replacement plan and swap only at callback boundary."""
        ordered=validate_patch(patch)
        targets={node['id']:node for node in patch['modules']}
        nodes={}
        # Reuse unchanged synth engines; no voice resets on a cable reconnect.
        for nid in ordered:
            item=targets[nid]
            prior=self.nodes.get(nid)
            if prior and prior.type==item['type']:
                nodes[nid]=prior
            else:
                nodes[nid]=NodeRuntime(item,self.engine_factory,self.sr)
        links=[]
        for w in patch['connections']:
            sid,op=w['from'].rsplit(':',1)
            did,ip=w['to'].rsplit(':',1)
            kind=next(p.kind for p in DEFINITIONS[targets[sid]['type']]['outs'] if p.name==op)
            links.append(Link(sid,op,did,ip,kind))
        self.queue.put(('plan',(nodes,tuple(ordered),tuple(links))))
        for nid in ordered:
            # A patch import is authoritative even for an existing node ID:
            # keep its DSP state but synchronize every saved control and mode.
            self.queue.put(('sync_config',(nid,targets[nid],patch.get('format')=='wirescape.visual.study')))
        for nid in ordered:
            node=targets[nid]
            if node['type']=='sequencer':
                from modules.sequencer.engine import validate_pattern,default_pattern
                self.queue.put(('sequence',(nid,validate_pattern(node.get('sequence',default_pattern())))))
            adv=node.get('advanced',{}) if isinstance(node.get('advanced',{}),dict) else {}
            raw=node.get('raw',{}) if isinstance(node.get('raw',{}),dict) else {}
            self.queue.put(('sync_overrides',(nid,adv,raw)))
        # Lookup for control thread only; sync to audio thread via queue above.
        self.nodes=nodes
        return list(ordered)

    def control(self,id,name,value):
        node=self.nodes.get(id)
        if not node: raise PatchError('Unknown node id')
        value=normalize_control(node.type,name,value)
        self.queue.put(('control',(id,name,value)))

    def monitor(self,id,on):
        node=self.nodes.get(id)
        if not node or node.type!='interface':raise PatchError('Monitor is only available on Audio I/O')
        self.queue.put(('monitor',(id,bool(on))))

    def mode(self,id,index):
        node=self.nodes.get(id)
        if not node:raise PatchError('Unknown node id')
        if not DEFINITIONS[node.type]['modes']:raise PatchError('Module has no mode')
        index=int(index)
        if not (0<=index<len(DEFINITIONS[node.type]['modes'])):raise PatchError('Invalid mode')
        self.queue.put(('mode',(id,index)))

    def advanced(self,id,name,value,clear=False):
        import modules.chaos.macros as M
        if name not in M.SPEC:raise PatchError('Unknown CHAOS macro '+str(name))
        if id not in self.nodes or self.nodes[id].type!='chaos':raise PatchError('Not a CHAOS node')
        if not clear:
            try: value=M.clamp(name,float(value))
            except (ValueError,TypeError):raise PatchError('Invalid macro value')
        self.queue.put(('advanced',(id,name,value,clear)))

    def raw(self,id,voice,key,normalized,clear=False):
        import modules.chaos.voices as V
        allowed={k for _,names in V.RAW_GROUPS for k in names}
        allowed.update(full_raw_keys())
        if key not in allowed or voice not in ('A','B'):
            raise PatchError('Invalid raw Surge control')
        if id not in self.nodes or self.nodes[id].type!='chaos':
            raise PatchError('Not a CHAOS node')
        value=max(0.0,min(1.0,float(normalized)))
        if not math.isfinite(value):raise PatchError('Raw control must be finite')
        self.queue.put(('raw',(id,voice,key,value,clear)))

    def keyboard(self,id,note,on):
        if id not in self.nodes or self.nodes[id].type!='chaos':raise PatchError('Not a CHAOS node')
        note=int(note)
        if not (0<=note<=127):raise PatchError('MIDI note outside 0..127')
        self.queue.put(('note',(id,note,bool(on))))

    def panic(self):
        self.queue.put(('panic',None))

    def sequence(self,id,data):
        from modules.sequencer.engine import validate_pattern
        node=self.nodes.get(id)
        if not node or node.type!='sequencer':raise PatchError('Not a sequencer node')
        pattern=validate_pattern(data)
        self.queue.put(('sequence',(id,pattern)))

    @staticmethod
    def _set_note(node,note,owner,on):
        """Called on callback thread. Source ownership prevents overlapping
        sequencers, gate and keyboard from stealing each other's releases."""
        owners=node.note_owners.get(note)
        if not on and (owners is None or owner not in owners):return
        if owners is None:
            owners=set()
            node.note_owners[note]=owners
        if on:
            if owner not in owners:
                if not owners:node.synth.note_on(note)
                owners.add(owner)
        else:
            owners.discard(owner)
            if not owners:
                node.note_owners.pop(note,None)
                node.synth.note_off(note)

    def _purge_sources(self,old,active,links):
        allowed={(l.dst,l.src) for l in links if l.kind=='notes'}
        for nid,node in old.items():
            if node.type!='chaos' or not node.synth:continue
            if active.get(nid) is not node:
                node.synth.panic()
                node.note_owners.clear()
                continue
            for pitch,owners in list(node.note_owners.items()):
                for owner in list(owners):
                    if owner[0]=='sequence' and (nid,owner[1]) not in allowed:
                        self._set_note(node,pitch,owner,False)

    def _commands(self):
        # Only called from audio thread. No direct UI thread DSP writes.
        while True:
            try: op, info=self.queue.get_nowait()
            except queue.Empty:break
            if op=='plan':
                old=getattr(self,'active',{})
                self.active,self.order,self.links=info
                self._purge_sources(old,self.active,self.links)
            elif op=='panic':
                for node in getattr(self,'active',{}).values():
                    if node.synth:
                        node.synth.panic()
                        node.note_owners.clear()
                    if node.type=='sequencer':node.plugin.stop(halt=True)
            else:
                active=getattr(self,'active',{})
                node=active.get(info[0])
                if not node:continue
                if op=='sync_config':
                    node_data=info[1]
                    node.controls=dict(DEFINITIONS[node.type]['controls'])
                    node.controls.update({k:normalize_control(node.type,k,v) for k,v in node_data.get('values',{}).items() if k in node.controls})
                    opts=DEFINITIONS[node.type]['modes']
                    node.mode=max(0,min(len(opts)-1,int(node_data.get('mode',0)))) if opts else 0
                    node.monitor=bool(node_data.get('monitor',False)) and not info[2]
                    node.last_macros={}
                    node.last_noise=None
                    if node.type=='sequencer':node.plugin.mode=node.mode
                elif op=='sync_overrides':
                    import modules.chaos.macros as M
                    import modules.chaos.voices as V
                    _,adv,raw=info
                    if node.type!='chaos':continue
                    old_raw=set(node.raw_overrides)
                    node.advanced={k:M.clamp(k,float(v)) for k,v in adv.items() if k in M.SPEC and math.isfinite(float(v))}
                    allowed={key for _,keys in V.RAW_GROUPS for key in keys}
                    allowed.update(full_raw_keys())
                    node.raw_overrides={}
                    for key in old_raw:
                        if hasattr(node.synth,'q'):send_raw(node.synth,key[0],key[1],None,clear=True)
                    for label,v in raw.items():
                        if not isinstance(label,str) or '|' not in label:continue
                        voice,key=label.split('|',1)
                        try: v=float(v)
                        except (TypeError,ValueError):continue
                        if voice in ('A','B') and key in allowed and math.isfinite(v):
                            v=max(0,min(1,v));node.raw_overrides[(voice,key)]=v
                            if hasattr(node.synth,'q'):send_raw(node.synth,voice,key,v)
                elif op=='control':
                    node.change(info[1],info[2])
                    if node.type=='chaos':
                        for key in FRONT_OWNS.get(info[1],()):node.advanced.pop(key,None)
                elif op=='mode':
                    node.mode=info[1]
                    if node.type=='sequencer':
                        node.plugin.mode=node.mode
                        node.plugin.stop()
                elif op=='sequence':node.plugin.configure(info[1])
                elif op=='monitor':node.monitor=info[1]
                elif op=='advanced':
                    _,key,value,clear=info
                    if clear:node.advanced.pop(key,None)
                    else:node.advanced[key]=value
                elif op=='raw':
                    _,voice,key,value,clear=info
                    if clear:
                        node.raw_overrides.pop((voice,key),None)
                        if node.synth and hasattr(node.synth,'q'):
                            send_raw(node.synth,voice,key,None,clear=True)
                    else:
                        node.raw_overrides[(voice,key)]=value
                        if node.synth and hasattr(node.synth,'q'):
                            send_raw(node.synth,voice,key,value)
                elif op=='note':
                    _,note,on=info
                    if node.synth:
                        self._set_note(node,note,('keyboard',None),on)

    def process(self,n=BLOCK,input_audio=None):
        t0=time.perf_counter()
        if n <= 0: raise ValueError('n must be positive')
        self._commands()
        # Noncanonical frame sizes: standalone render still works for mock mods;
        # the native Surge callback is configured to deliver exactly 512 frames.
        if input_audio is None:device_in=np.zeros((2,n),np.float32)
        else:
            device_in=np.asarray(input_audio,dtype=np.float32)
            if device_in.shape != (2,n):raise ValueError('input_audio shape must be (2,frames)')
            device_in=np.nan_to_num(device_in,nan=0.0,posinf=0.0,neginf=0.0)
        active=getattr(self,'active',{})
        results={}
        inputs=defaultdict(list)
        for link in self.links:
            inputs[link.dst,link.inport].append(link)
        def obtain(nid,key,kind):
            incoming=inputs[nid,key]
            if kind in ('audio',):
                if not incoming:return np.zeros(n,dtype=np.float32)
                return sum((np.asarray(results[l.src].get(l.out,np.zeros(n,dtype=np.float32)),dtype=np.float32) for l in incoming),np.zeros(n,dtype=np.float32))
            if kind in ('mod',):
                return float(np.clip(sum(float(results[l.src].get(l.out,0.0)) for l in incoming),-1.0,1.0))
            if kind=='notes':
                return [(l.src,event) for l in incoming for event in results[l.src].get(l.out,())]
            return any(bool(results[l.src].get(l.out,False)) for l in incoming)
        master=np.zeros((2,n),np.float32)
        for nid in self.order:
            node=active[nid]
            if node.type=='interface':
                gain=2*node.controls['INPUT GAIN']/100
                results[nid]={'extL':device_in[0]*gain,'extR':device_in[1]*gain}
                continue
            if node.type=='modulator':
                results[nid]=node.mod.process(n,node.controls,node.mode,
                    rate_cv=obtain(nid,'rate','mod'),clock=obtain(nid,'clock','gate'),
                    reset=obtain(nid,'reset','gate'))
                continue
            if node.plugin is not None:
                spec=DEFINITIONS[node.type]
                values={p.name:obtain(nid,p.name,p.kind) for p in spec['ins']}
                output=node.plugin.process(n,node.controls,values)
                if not isinstance(output,dict):raise RuntimeError('Plugin must return port mapping')
                clean={}
                for port in spec['outs']:
                    item=output.get(port.name)
                    if port.kind=='audio':
                        v=np.asarray(item,dtype=np.float32)
                        if v.shape!=(n,):raise RuntimeError('Plugin returned wrong audio block size')
                        clean[port.name]=np.nan_to_num(v,nan=0,posinf=0,neginf=0)
                    elif port.kind=='mod':
                        value=float(item)
                        if not math.isfinite(value):value=0.0
                        clean[port.name]=max(-1,min(1,value))
                    elif port.kind=='notes':
                        if not isinstance(item,(list,tuple)) or len(item)>128:
                            raise RuntimeError('Invalid note-event output')
                        clean[port.name]=[]
                        for event in item:
                            if not isinstance(event,(list,tuple)) or len(event)!=2 or type(event[0])!=int or not 0<=event[0]<=127 or type(event[1])!=bool:
                                raise RuntimeError('Invalid note-event payload')
                            clean[port.name].append(tuple(event))
                    else:clean[port.name]=bool(item)
                results[nid]=clean
                continue
            if node.type=='chaos':
                gate=obtain(nid,'gate','gate')
                if gate!=node.gate_high:
                    node.gate_high=gate
                    # Legacy gate convention uses C3 (MIDI 48).
                    self._set_note(node,48,('gate',None),gate)
                for source,(pitch,on) in obtain(nid,'notes','notes'):
                    self._set_note(node,pitch,('sequence',source),on)
                controls=dict(node.controls)
                for port,param in (('pitch','PITCH'),('morph','MORPH'),('chaos','CHAOS'),('cutoff','CUTOFF')):
                    controls[param]=float(np.clip(controls[param]+obtain(nid,port,'mod')*30.0,0,100))
                macros=chaos_macros(controls,node.mode)
                macros.update(node.advanced)
                # Avoid unnecessarily re-sending each parameter every 512 frames.
                delta={k:v for k,v in macros.items() if node.last_macros.get(k)!=v}
                if delta:
                    node.synth.set_many(delta)
                    node.last_macros=dict(macros)
                if node.mode==2:
                    desired=0.32
                else:
                    desired=0.0
                if desired!=node.last_noise:
                    if hasattr(node.synth,'q'):
                        node.synth.q.put(('raw','A',('Noise Volume',desired)))
                    node.last_noise=desired
                ext=obtain(nid,'audio','audio')
                extblock=np.stack((ext,ext)) if np.any(ext) else None
                if extblock is not None and 'EXT_MIX' not in node.advanced:
                    # EXTERNAL input active means it is used at the body input.
                    if node.last_macros.get('EXT_MIX')!=0.6:
                        node.synth.set_many({'EXT_MIX':0.6})
                        node.last_macros['EXT_MIX']=0.6
                output=np.asarray(node.synth.render(n,input_block=extblock),dtype=np.float32)
                if output.shape!=(2,n):raise RuntimeError('Synth must render (2,n) audio')
                output=np.nan_to_num(output,nan=0.0,posinf=0.0,neginf=0.0)
                results[nid]={'left':output[0],'right':output[1],
                              'mod':float(np.clip(np.sqrt(np.mean(output*output))*2,0,1))}
        for nid in self.order:
            node=active[nid]
            if node.type!='interface':continue
            left=obtain(nid,'inL','audio')
            right=obtain(nid,'inR','audio')
            target_gain=node.controls['OUTPUT LEVEL']/100
            # Smooth output automation to reduce clicks from knobs and modulation.
            node.output_gain+=0.25*(target_gain-node.output_gain)
            master+=np.stack((left,right))*node.output_gain
            if node.monitor:
                master+=device_in*(2*node.controls['INPUT GAIN']/100)*node.output_gain
        # Soft safety ceiling; not a mastering limiter or a guarantee against CPU xruns.
        master=np.tanh(master*1.15)/math.tanh(1.15)
        master=np.asarray(master,dtype=np.float32)
        master=np.nan_to_num(master,nan=0.0,posinf=0.0,neginf=0.0)
        elapsed=(time.perf_counter()-t0)*1000
        # atomic assignment of immutable report dict for web status thread
        self.report={'peak_l':float(np.max(np.abs(master[0]))),'peak_r':float(np.max(np.abs(master[1]))),
                     'rms_l':float(np.sqrt(np.mean(master[0]**2))),'rms_r':float(np.sqrt(np.mean(master[1]**2))),
                     'render_ms':elapsed,'load':elapsed/(n/self.sr*1000),'modules':len(active),
                     'sequencers':{nid:node.plugin.step for nid,node in active.items() if node.type=='sequencer'}}
        return master
