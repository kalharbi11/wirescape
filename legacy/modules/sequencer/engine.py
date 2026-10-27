"""Block-scheduled harmonic sequencer (no UI or network code in audio callback).

Transport is in sample frames, but output events are delivered at callback boundaries.
The CHAOS engine currently renders indivisible 512-frame blocks; true intra-block
sample accuracy requires a different synth/render contract. No I/O or locks here.
"""
import heapq
import math
import random

from wirescape.schema import PatchError

QUALITIES = {
    'maj': (0, 4, 7), 'min': (0, 3, 7), 'dim': (0, 3, 6),
    'aug': (0, 4, 8), 'sus2': (0, 2, 7), 'sus4': (0, 5, 7),
    '5': (0, 7), '6': (0, 4, 7, 9), 'm6': (0, 3, 7, 9),
    '7': (0, 4, 7, 10), 'maj7': (0, 4, 7, 11),
    'm7': (0, 3, 7, 10), 'mMaj7': (0, 3, 7, 11),
    'dim7': (0, 3, 6, 9), 'm7b5': (0, 3, 6, 10),
    'add9': (0, 4, 7, 14), 'madd9': (0, 3, 7, 14),
    '9': (0, 4, 7, 10, 14), 'maj9': (0, 4, 7, 11, 14),
    'm9': (0, 3, 7, 10, 14),
}
ROOTS = ('C','C#','D','D#','E','F','F#','G','G#','A','A#','B')
MAX_STEPS = 16


def number(value, low, high, label):
    if isinstance(value, bool):
        raise PatchError(label+' must be a number')
    try: v = float(value)
    except (TypeError, ValueError, OverflowError): raise PatchError(label+' must be a number')
    if not math.isfinite(v) or not low <= v <= high:
        raise PatchError(label+' must be between '+str(low)+' and '+str(high))
    return v


def integer(value, low, high, label):
    v = number(value, low, high, label)
    if not v.is_integer(): raise PatchError(label+' must be an integer')
    return int(v)


def default_step(i):
    # A immediately musical minor-key progression. All 16 slots are editable.
    roots = (0, 8, 3, 10)  # Cm7 -> Abmaj7 -> Fm7 -> Bb7
    qualities = ('m7', 'maj7', 'm7', '7')
    group = i // 4
    return dict(root=roots[group], octave=3, quality=qualities[group],
                inversion=0, spread=False, strum_ms=0, probability=1.0,
                enabled=i % 4 == 0, trig=i % 4 == 0, gate=0.85,
                duration_steps=4, every=1,
                mod=(-0.55, 0.4, -0.1, 0.7)[group])


def default_pattern():
    return {'length': 16, 'mod_length': 16, 'mod_division': 1,
            'steps': [default_step(i) for i in range(MAX_STEPS)],
            'modulation': [round(0.65*math.sin(i*math.tau/16), 3) for i in range(16)]}


def validate_pattern(data):
    """Bounded, canonical serialized state; reject arbitrary/malformed patches."""
    if not isinstance(data, dict): raise PatchError('Sequence must be an object')
    if set(data)-{'length','mod_length','mod_division','steps','modulation'}:
        raise PatchError('Unknown sequence field')
    raw = data.get('steps')
    if not isinstance(raw, list) or len(raw)!=MAX_STEPS:
        raise PatchError('Sequence must contain exactly 16 steps')
    normalized = []
    allowed = set(default_step(0))
    for i, step in enumerate(raw):
        if not isinstance(step,dict) or set(step)-allowed:
            raise PatchError('Invalid step at '+str(i+1))
        merged = {**default_step(i),**step}
        quality = merged['quality']
        if not isinstance(quality,str) or quality not in QUALITIES:
            raise PatchError('Unknown chord quality on step '+str(i+1))
        normalized.append({
            'root':integer(merged['root'],0,11,'root'),
            'octave':integer(merged['octave'],1,6,'octave'),
            'quality':quality,
            'inversion':integer(merged['inversion'],0,4,'inversion'),
            'spread':boolean(merged['spread'],'spread'),
            'strum_ms':integer(merged['strum_ms'],0,200,'strum_ms'),
            'probability':number(merged['probability'],0,1,'probability'),
            'enabled':boolean(merged['enabled'],'enabled'),
            'trig':boolean(merged['trig'],'trig'),
            'gate':number(merged['gate'],0.05,1,'gate'),
            'duration_steps':integer(merged['duration_steps'],1,16,'duration_steps'),
            'every':integer(merged['every'],1,8,'every'),
            'mod':number(merged['mod'],-1,1,'mod')
        })
    mod = data.get('modulation',default_pattern()['modulation'])
    if not isinstance(mod,list) or len(mod)!=MAX_STEPS:
        raise PatchError('Modulation lane requires 16 values')
    return {'length':integer(data.get('length',16),1,16,'length'),
            'mod_length':integer(data.get('mod_length',16),1,16,'mod_length'),
            'mod_division':integer(data.get('mod_division',1),1,8,'mod_division'),
            'steps':normalized,
            'modulation':[number(v,-1,1,'modulation') for v in mod]}


def boolean(value,label):
    if not isinstance(value,bool):raise PatchError(label+' must be true or false')
    return value


def chord_notes(step):
    root = 12 * (step['octave'] + 1) + step['root']
    pitches = [root + n for n in QUALITIES[step['quality']]]
    inv = min(step['inversion'],len(pitches)-1)
    for j in range(inv):pitches[j] += 12
    pitches.sort()
    if step['spread']:
        for j in range(len(pitches)-2,-1,-2):pitches[j]-=12
    return sorted(set(max(0,min(127,n)) for n in pitches))


class SequencerProcessor:
    """Generates note events (note,on), one-block trigger pulses and bipolar CV.

    Each instance owns a deterministic RNG and schedule. All state mutations
    are done via AudioGraph's audio-thread queue, not from the browser thread.
    """
    def __init__(self,sample_rate=48000):
        self.sr=sample_rate
        self.pattern=default_pattern()
        self.mode=0
        self.rng=random.Random(112)
        self.frame=0
        self.next_frame=0.0
        self.step=-1
        self.loop_count=0
        self.mod_step=-1
        self.last_clock=False
        self.last_reset=False
        self.pending=[]
        self.serial=0
        self.notes={}  # pitch -> number of overlapping note instances
        self.last_run=False
        self.halted=False
        self.phase_frame=0
        self.release=[]
        self.env_level=0.0
        self.current_mod=0.0
        self.step_mod=0.0

    def configure(self,pattern):
        self.pattern=validate_pattern(pattern)
        self.stop()
        self.halted=False

    def stop(self,halt=False):
        self.release.extend((n,False) for n in sorted(self.notes))
        self.notes.clear()
        self.pending=[]
        self.step=-1
        self.loop_count=0
        self.mod_step=-1
        self.next_frame=float(self.frame)
        self.phase_frame=self.frame
        self.halted |= halt
        self.env_level=0.0
        self.current_mod=0.0
        self.step_mod=0.0

    def reset(self):
        self.stop()

    def _push(self,when,note,on):
        heapq.heappush(self.pending,(when,self.serial,note,on))
        self.serial+=1

    def _tick(self,when,interval_frames,depth):
        pattern=self.pattern
        previous=self.step
        self.step=(self.step+1)%pattern['length']
        if self.step==0 and previous>=0:self.loop_count+=1
        step=pattern['steps'][self.step]
        # Independent modulation lane runs N times for each chord step.
        self.step_mod=step['mod']
        if step['enabled'] and self.loop_count%step['every']==0 and self.rng.random()<=step['probability']:
            notes=chord_notes(step)
            # Remaining active notes are allowed to sustain until their off events.
            # The same MIDI pitch will not be retriggered until its previous off.
            strum_frames=step['strum_ms']*self.sr/1000
            gate_frames=max(1,interval_frames*step['duration_steps']*step['gate'])
            for j,note in enumerate(notes):
                offset=j*strum_frames
                self._push(when+offset,note,True)
                self._push(when+offset+gate_frames,note,False)
        if step['trig']:
            self.env_level=1.0
            return True
        return False

    def process(self,n,controls,inputs):
        clock=bool(inputs.get('clock',False))
        reset=bool(inputs.get('reset',False))
        enabled=float(controls.get('RUN',100))>=50
        bpm=40+2*float(controls.get('BPM',50))
        beat_frames=self.sr*60/bpm
        step_frames=beat_frames/4  # 16th notes
        gate_out=False
        pulse=False
        events=self.release
        self.release=[]
        if reset and not self.last_reset:
            self.reset();self.halted=False
            events.extend(self.release);self.release=[]
        self.last_reset=reset
        if not enabled:
            if self.last_run or self.notes or self.pending:
                self.stop();events.extend(self.release);self.release=[]
        elif not self.halted:
            if not self.last_run:
                self.next_frame=float(self.frame)
                self.phase_frame=self.frame
            if self.mode==0:  # internal clock, 16th-note base
                limit=self.frame+n
                while self.next_frame < limit:
                    idx=(self.step+1)%self.pattern['length']
                    swing=float(controls.get('SWING',0))/100*0.49
                    # Alternate intervals preserve the two-step average tempo.
                    span=step_frames*(1+(swing if idx%2==0 else -swing))
                    pulse |= self._tick(self.next_frame,span,float(controls.get('DEPTH',100))/100)
                    self.next_frame+=span
            elif clock and not self.last_clock:
                pulse=self._tick(self.frame,step_frames,float(controls.get('DEPTH',100))/100)
        if enabled and not self.last_run and self.halted:
            self.halted=False  # RUN was explicitly cycled after panic
            self.next_frame=float(self.frame)
        self.last_run=enabled
        self.last_clock=clock
        # Every mod_division subdivisions per chord step; phase is sample-derived.
        # In external clock mode, the lane advances per incoming clock edge.
        division=self.pattern['mod_division']
        if self.mode==0:
            phase=int((self.frame-self.phase_frame)/(step_frames/max(1,division)))
        else:
            phase=max(0,self.step)*division
        self.mod_step=phase%self.pattern['mod_length']
        self.current_mod=self.pattern['modulation'][self.mod_step]
        cutoff_mod=max(-1,min(1,(self.current_mod+self.step_mod)*0.5*float(controls.get('DEPTH',100))/100)) if enabled and not self.halted else 0.0
        # Events due during this block are dispatched at its start in CHAOS,
        # preserving their order but not sub-block timing.
        while self.pending and self.pending[0][0]<self.frame+n:
            _,_,note,on=heapq.heappop(self.pending)
            if on:
                count=self.notes.get(note,0)
                self.notes[note]=count+1
                if count:continue
            else:
                count=self.notes.get(note,0)
                if not count:continue
                if count>1:
                    self.notes[note]=count-1
                    continue
                del self.notes[note]
            events.append((note,on))
        gate_out=bool(self.notes)
        decay=0.025+2.0*float(controls.get('DECAY',35))/100
        out_env=self.env_level
        self.env_level*=math.exp(-n/self.sr/decay)
        if self.env_level<0.0001:self.env_level=0.0
        self.frame+=n
        return {'notes':events,'gate':gate_out,'trig':pulse,
                'mod':cutoff_mod,'env':max(0,min(1,out_env))}
