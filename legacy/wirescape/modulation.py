"""Block-rate modulation; deterministic with a supplied seed, no DSP hardware dependencies."""
import math, random

class Modulator:
    def __init__(self, sr=48000, seed=11):
        self.sr=sr
        self.rng=random.Random(seed)
        self.phase=0.0
        self.hold=self.rng.uniform(-1,1)
        self.smooth=self.hold
        self.last_reset=False
        self.last_gate=False

    def process(self, n, controls, mode=0, rate_cv=0, clock=False, reset=False):
        if reset and not self.last_reset: self.phase=0.0
        self.last_reset=bool(reset)
        # rate 0.03..12 Hz, exponential frequency response
        rate=0.03*(400 ** (float(controls.get('RATE',44))/100))
        rate*=2 ** (2*max(-1,min(1,rate_cv)))
        rate=max(0.005,min(18,rate))
        previous=self.phase
        self.phase=(self.phase+(n/self.sr)*rate)%1.0
        wrapped=self.phase < previous
        if wrapped or (clock and not self.last_gate):
            self.hold=self.rng.uniform(-1,1)
        self.last_gate=bool(clock)
        drift=float(controls.get('DRIFT',29))/100
        random_amt=float(controls.get('RANDOM',34))/100
        self.smooth+=(self.hold-self.smooth)*min(1,0.045+drift*0.2)
        p=self.phase
        shape=float(controls.get('SHAPE',53))/100
        sine=math.sin(2*math.pi*p)
        tri=1-4*abs(p-0.5)
        basic=sine*(1-shape)+tri*shape
        depth=float(controls.get('DEPTH',67))/100
        if mode==0:
            a=(1-random_amt)*basic+random_amt*self.smooth
            b=(1-random_amt)*math.cos(2*math.pi*p)+random_amt*self.hold
        elif mode==1:
            a=self.smooth
            b=self.hold
        else: # Envelope approximation: periodic attack then exponential decay
            a=math.exp(-8*p)
            b=math.exp(-4*p)
        a=max(-1,min(1,a*depth))
        b=max(-1,min(1,b*depth))
        return {'modA': a,'modB': b,'gate':bool(p<0.06)}
