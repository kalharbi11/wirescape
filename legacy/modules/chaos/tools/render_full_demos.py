"""Real Surge-based demo renders, requires compiled surgepy. Output is WAV.\nRun: python tools/render_full_demos.py\n"""
import sys,time
from pathlib import Path
import numpy as np
from scipy.io import wavfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from engine import Engine,SR
out=ROOT/'renders'/'full_chaos';out.mkdir(parents=True,exist_ok=True)
variants={
    '01_original_clean': {},
    '02_scream': {'BREAK':.7,'ROUGH_DEPTH':.5,'ROUGH_RATE':70},
    '03_unstable': {'DETUNE':22,'VOICES':5,'DRIFT':.7,'JITTER':.55,'FALL':.3},
    '04_resonant_body': {'BODY_MIX':.7,'BODY_FEED':.6,'BODY_TONE':.35},
    '05_two_layer_ring': {'B_MUTE':0,'ROUTE_MODE':1,'ROUTE_DEPTH':.7},
}
for name,params in variants.items():
    e=Engine()
    e.set_many(params)
    t=time.perf_counter()
    y=e.render_offline(3.0,notes=(45,),release_at=2.0)
    secs=y.shape[1]/SR
    speed=secs/(time.perf_counter()-t)
    assert np.isfinite(y).all()
    peak=float(np.max(np.abs(y)))
    assert peak < .92, (name,peak)
    rms=float(np.sqrt(np.mean(y**2)))
    if rms < 1.e-4:
        print('WARNING: unusually quiet',name)
    wavfile.write(out/(name+'.wav'),SR,(np.clip(y.T,-1,1)*32767).astype('int16'))
    print(f'{name} | speed {speed:.2f}x realtime | rms {rms:.4f} peak {peak:.3f}')
