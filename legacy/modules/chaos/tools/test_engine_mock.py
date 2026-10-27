"""End-to-end engine contract test with a deliberately minimal fake surgepy.
Verifies removal doesn't leave broken references in mixer, queue, or rendering.
NOT a substitute for running compiled Surge XT and listening to the output.
"""
import sys,types
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import macros,voices

names={k for _,k,_ in macros.INIT_VOICE}
names.update(k for _,k,_ in macros.surge_targets(macros.DEFAULTS))
names.update(k for _,keys in voices.RAW_GROUPS for k in keys if not k.startswith('MOD '))
for rec in voices.B_TYPES.values():names.update(rec)

class Ref:
    def __init__(self,name,id):self.name=name;self.id=id
    def getName(self):return self.name
    def getId(self):return self
    def getSynthSideId(self):return self.id
class Entry:
    def __init__(self,name,ref):self.ref=ref;self.name=name
    def getScene(self):return 0 if self.name.startswith(('FX ','Polyphony Limit')) else 1
    def getParams(self):return [self.ref]
class Group:
    def __init__(self,entries):self.entries=entries
    def getEntries(self):return self.entries
class Synth:
    def __init__(self,sr):
        self.sr=sr;self.bs=32;self.p={};self.notes=set();self.phase=0
        self.refs={name:Ref(name,i*12) for i,name in enumerate(sorted(names),1)}
        self.entries=[Entry(k,v) for k,v in self.refs.items()]
    def getBlockSize(self):return self.bs
    def getControlGroup(self,k):return Group(self.entries if k==0 else [])
    def setParamVal(self,p,v):self.p[p.name]=v
    def getParamVal(self,p):return self.p.get(p.name,0)
    def getParamMin(self,p):return 0
    def getParamMax(self,p):return 1
    def getParamValType(self,p):return 'float'
    def getParamDisplay(self,p):return str(self.getParamVal(p))
    def getParameterName(self,p):return p.name
    def setModDepth01(self,*args):pass
    def getModSource(self,s):return s
    def playNote(self,ch,n,vel,id):self.notes.add(n)
    def releaseNote(self,ch,n,id):self.notes.discard(n)
    def allNotesOff(self):self.notes.clear()
    def process(self):pass
    def loadSCLFile(self,path):pass
    def retuneToStandardTuning(self):pass
    def pitchBend(self,*args):pass
    def createMultiBlock(self,nb):return np.zeros((2,nb*32),np.float32)
    def processMultiBlock(self,mb):
        n=mb.shape[1];t=(self.phase+np.arange(n))/self.sr
        f=220*2**((min(self.notes)-57)/12) if self.notes else 0
        mb[:]=(.12*np.sin(2*np.pi*f*t)).astype('float32') if self.notes else 0
        self.phase+=n
sys.modules['surgepy']=types.SimpleNamespace(createSurge=lambda sr:Synth(sr),constants=types.SimpleNamespace(ms_lfo1=1,ms_lfo2=2,ms_velocity=3))
import engine
E=engine.Engine()
E.note_on(57)
y=E.render();assert y.shape==(2,engine.BLOCK) and np.max(np.abs(y))>0
E.set_many({'B_MUTE':0,'ROUTE_MODE':1,'ROUTE_DEPTH':.8,'BODY_MIX':.7,'BODY_FEED':.5,'EXT_MIX':.8})
y2=E.render(input_block=np.ones((2,engine.BLOCK),np.float32)*.05)
assert y2.shape==(2,engine.BLOCK) and np.isfinite(y2).all()
assert set(E.last_stems)=={'A','B','CHAOS'}
assert np.max(np.abs(E.last_stems['CHAOS']))>0
E.q.put(('loop_freeze',True,None))
E.render();assert E.looper.frozen
E.note_off(57)
E.render();assert E.held==[]
E.panic();E.render();assert not E.sounding
assert E.fall_cents==0
print('PASS: mock two-Surge engine, queue, external port, routing, body, looper, limiter, voice events')
