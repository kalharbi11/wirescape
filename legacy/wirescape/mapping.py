"""CHAOS performance panel -> existing original CHAOS macro DSP.

30 UI knobs, two Surge engines, no native Surge changes. All mapping is
centralized here and verified in tests. Deep macro overrides are applied AFTER
this layer by AudioGraph; raw Surge overrides are applied inside CHAOS Engine.
"""
import math
from .schema import normalize_control

CHAOS_MOD_DEST = {'pitch':'PITCH','morph':'MORPH','chaos':'CHAOS'}

# A front-panel gesture takes ownership of the corresponding original macros.
# Deep/loaded preset edits remain authoritative until the performer touches the
# corresponding front control; untouched other macro overrides remain intact.
FRONT_OWNS={
    'MORPH':('A_LEVEL','B_LEVEL','A_MUTE','B_MUTE'),
    'CHAOS':('JITTER','ROUGH_DEPTH','DRIFT'),
    'FOLD':('BREAK','DRIVE_TYPE'),
    'CUTOFF':('CUTOFF',),
    'PITCH':('OFFSET',), 'DETUNE':('DETUNE','VOICES'),
    'CLUSTER':('CLUSTER',), 'SUB':('SUB',), 'FM':('FM_RATIO',),
    'ROUGH':('ROUGH_DEPTH',), 'JITTER':('JITTER',),
    'DRIFT':('DRIFT',), 'FALL':('FALL',),
    'COUPLE':('ROUTE_MODE','ROUTE_DEPTH'),
    'BODY':('BODY_MIX',), 'TONE':('BODY_TONE',), 'FEEDBACK':('BODY_FEED',),
    'RATE':('LFO_RATE',), 'MOTION':('LFO_DEPTH','LFO_SELF'),
    'TAPE':('TAPE',), 'SPACE':('SPACE','SPACE_MIX'),
    'LOOP':('LOOP_MIX','LOOP_FEEDBACK'),
    # ADSR controls address the raw Surge envelope for each instance, not macros.
    **{f'{voice}_{stage}':() for voice in ('A','B') for stage in ('ATTACK','DECAY','SUSTAIN','RELEASE')},
}

ENVELOPE_CONTROLS = {
    f'{voice}_{stage}':(voice, f'Amp EG {stage.title()}')
    for voice in ('A','B') for stage in ('ATTACK','DECAY','SUSTAIN','RELEASE')
}


# Front controls are serialized as 0..100. The host owns their engineering
# units and intentionally limits high-polyphony voicing on the performance page.
def chaos_macros(controls, mode=0):
    def f(name):
        return normalize_control('chaos', name, controls.get(name, 50)) / 100.0

    morph=f('MORPH'); chaos=f('CHAOS'); fold=f('FOLD')
    pitch=f('PITCH'); detune=f('DETUNE')
    coupled=f('COUPLE'); fm=f('FM'); motion=f('MOTION')
    a=max(.0001,math.cos(morph*math.pi/2))
    b=max(.0001,math.sin(morph*math.pi/2))
    result={
        'A_LEVEL':max(-48.,20*math.log10(a)),
        'B_LEVEL':max(-48.,20*math.log10(b)),
        'A_MUTE':int(morph >= .999), 'B_MUTE':int(morph <= .001),
        'BREAK':fold, 'DRIVE_TYPE':4,
        'CUTOFF':f('CUTOFF'),
        'DETUNE':detune*60., 'VOICES':3 if detune>.02 else 1,
        'OFFSET':(pitch-.5)*100.,
        'CLUSTER':1+int(round(f('CLUSTER')*4)),
        'SUB':f('SUB'),
        'FM_RATIO':fm,
        'ROUGH_DEPTH':max(chaos*.72,f('ROUGH')),
        'JITTER':max(chaos*.72,f('JITTER')),
        'DRIFT':max(chaos*.58,f('DRIFT')),
        'FALL':f('FALL'),
        'ROUTE_DEPTH':coupled,
        'BODY_MIX':f('BODY')*.82,
        'BODY_TONE':f('TONE'),
        'BODY_FEED':f('FEEDBACK')*.76,
        # A moving low-frequency modulation of the original CUTOFF macro.
        'LFO_RATE':.05 * (160 ** f('RATE')),
        'LFO_DEPTH':motion,
        'LFO_SELF':motion*.38,
        'LFO_TARGET':'CUTOFF' if motion>.001 else 'off',
        'TAPE':f('TAPE'),
        'SPACE':.25+f('SPACE')*.75,
        'SPACE_MIX':f('SPACE')*.65,
        'LOOP_MIX':f('LOOP')*.8,
        'LOOP_FEEDBACK':f('LOOP')*.7,
    }
    if mode==0: # CLUSTER: two or more adjacent notes per key even with CLUSTER at zero
        result['CLUSTER']=max(2,result['CLUSTER'])
        result['ROUTE_MODE']=1 if coupled>.001 else 0
    elif mode==1: # FM: always engage inharmonic oscillator interaction and delay bend
        result['FM_RATIO']=max(.38,fm,chaos*.78)
        # FM Depth in the existing Surge mapping is driven by BREAK; a pure
        # ratio shift at BREAK=0 is nearly inaudible. Apply a scene floor.
        result['BREAK']=max(.36,fold)
        result['B_LEVEL']=max(-15.,result['B_LEVEL'])
        result['B_MUTE']=0
        result['ROUTE_MODE']=2
        result['ROUTE_DEPTH']=max(.32,coupled)
    elif mode==2: # NOISE: source is injected at the raw Surge oscillator
        result['ROUTE_MODE']=1 if coupled>.001 else 0
    else:raise ValueError('Invalid CHAOS mode')
    return result
