"""Small DSP-only listening comparisons. No surgepy required.

These deliberately use test sine oscillators rather than impersonating the
Surge instrument. For full CHAOS demos run tools/render_full_demos.py after
building surgepy.
"""
import sys
from pathlib import Path
import numpy as np
from scipy.io import wavfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from macros import DEFAULTS
from chaos_dsp import VoiceRouting,ResonantBody
from hostdsp import Output
SR=48000;N=512
OUT=ROOT/'renders'/'core_previews';OUT.mkdir(parents=True,exist_ok=True)
for name,mode in [('01_parallel',0),('02_ring_interaction',1),('03_delay_bend',2)]:
    routing=VoiceRouting(SR);res=ResonantBody(SR);limiter=Output();limiter.gain_db=0.0
    st=dict(DEFAULTS);st.update(ROUTE_MODE=mode,ROUTE_DEPTH=0.8,BODY_MIX=0.6,BODY_TONE=0.45,BODY_FEED=0.65)
    blocks=[]
    for j in range(int(4*SR/N)):
        t=(j*N+np.arange(N))/SR
        env=np.minimum(1,np.minimum(t/.2,(4-t)/.5))
        a=(.19*env*(np.sin(2*np.pi*110*t)+.26*np.sin(2*np.pi*220*t))).astype('float32')
        b=(.16*env*np.sin(2*np.pi*(164.81*t+0.02*np.sin(2*np.pi*.3*t)))).astype('float32')
        aa=np.stack((a,.85*a));bb=np.stack((.8*b,b))
        v=routing.process(aa,bb,st)
        v=res.process(v,st)
        blocks.append(limiter.process(v))
    y=np.concatenate(blocks,axis=1).T
    assert np.isfinite(y).all()
    wavfile.write(OUT/(name+'.wav'),SR,(np.clip(y,-1,1)*32767).astype('int16'))
    print(name,'rms',round(float(np.sqrt(np.mean(y**2))),3))
print('Previews saved to',OUT)
