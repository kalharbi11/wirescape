"""Independent regression and timing checks. Does not require surgepy, audio IO or GUI.
Run from project root: python tools/test_core.py
"""
import sys, time, json
from pathlib import Path
import numpy as np
HERE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HERE))
from macros import DEFAULTS, LIMITS, SPEC
from modlab import MATRIX_SOURCES, MATRIX_TARGETS, ModulationMatrix, Randomizer, GROUPS
from chaos_dsp import VoiceRouting, ResonantBody
from hostdsp import RoughAM, Looper, Output

N, SR = 512, 48000
n = np.arange(N) / SR
a = np.array([.25 * np.sin(2*np.pi*180*n)]*2, np.float32)
b = np.array([.22 * np.sin(2*np.pi*71*n)]*2, np.float32)
s = dict(DEFAULTS)
assert all(len(x)==3 and x[0] <= x[2] <= x[1] for x in SPEC.values())
assert not any('THROAT' in k or 'VOC_' in k or 'FRY_' in k or k.startswith(('C_', 'INS_', 'M_')) for k in s)
assert set(GROUPS) == {'SCREAM','UNSTABLE','DECAY','LOOPER','ROUTING','MODULATION'}
assert len(MATRIX_SOURCES)==5 and len(MATRIX_TARGETS)==12

r = VoiceRouting(SR)
s['ROUTE_DEPTH']=0
for mode in range(3):
    s['ROUTE_MODE']=mode
    assert np.allclose(r.process(a,b,s), a+b)

s['ROUTE_DEPTH']=0.9
results=[]
for mode in range(3):
    s['ROUTE_MODE']=mode
    out=r.process(a,b,s)
    assert np.isfinite(out).all() and out.shape==(2,N)
    results.append(out)
assert np.mean(np.abs(results[0]-results[1]))>.005
assert np.mean(np.abs(results[0]-results[2]))>.005

body=ResonantBody(SR)
s['BODY_MIX']=0.7;s['BODY_TONE']=.6;s['BODY_FEED']=.85
start=time.perf_counter()
for k in range(120):
    z=body.process(results[k%3],s)
    assert np.isfinite(z).all() and float(np.max(np.abs(z)))<5.0
elapsed=time.perf_counter()-start
speed=120*N/SR/elapsed
# Performance is reported, not asserted: different OS/CPUs vary hugely.
print(f'BODY DSP offline speed: {speed:.1f}x realtime (no Surge)')

s['MOD1_SRC']=2;s['MOD1_DST']=3;s['MOD1_AMT']=1
m=ModulationMatrix();m.hold=.75
new=m.apply(s,N/SR,0,LIMITS)
assert new['CUTOFF']>s['CUTOFF']
s['MOD1_AMT']=-1
new=m.apply(s,N/SR,0,LIMITS)
assert new['CUTOFF']<s['CUTOFF']
s['MOD1_SRC']=0
assert m.apply(s,N/SR,0,LIMITS)['CUTOFF']==s['CUTOFF']
rnd=Randomizer(LIMITS)
next_state=rnd.roll(s,'UNSTABLE','MID')
assert set(next_state)==set(s) and rnd.undo(next_state)==s
for f in (HERE/'presets').glob('*.json'):
    loaded=json.loads(f.read_text())
    for key,val in loaded.get('macros',{}).items():
        assert key in DEFAULTS, f'{f.name}: orphan key {key}'
        assert LIMITS[key][0]<=val<=LIMITS[key][1],f'{f.name}: {key} out of bounds'

out=Output();out.gain_db=0.0
limited=out.process(np.ones((2,N),np.float32)*100)
assert np.isfinite(limited).all() and np.max(np.abs(limited))<=out.CEIL+1e-6
print('PASS: two generator buses, three routes, resonator, mod matrix, randomness, legacy presets, limiter')
