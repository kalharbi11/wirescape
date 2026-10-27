"""Trusted installed plugin registry. No patch JSON ever imports executable code.

The extension loader discovers module.json as data; user-enabled Python code
registers processors and validated per-instance state. Legacy built-ins remain.
"""
import numpy as np
from .schema import DEFINITIONS, Port

AUDIO_PROCESSORS={}
STATE_VALIDATORS={}
STATE_DEFAULTS={}


def register_audio_module(identifier,definition,processor, *, default_state=None, validate_state=None):
    if not identifier.isidentifier() or identifier in DEFINITIONS:
        raise ValueError('Duplicate or invalid plugin identifier')
    for field in ('ins','outs','controls','modes'):
        if field not in definition:raise ValueError('Plugin missing '+field)
    for p in definition['ins']:
        if p.direction!='in':raise ValueError('Input port declares wrong direction')
    for p in definition['outs']:
        if p.direction!='out':raise ValueError('Output port declares wrong direction')
    if set(p.name for p in definition['ins']+definition['outs']).__len__()!=len(definition['ins']+definition['outs']):
        raise ValueError('Duplicate port name')
    if default_state is not None and (not callable(default_state) or not callable(validate_state)):
        raise ValueError('Stateful plugins need callable default_state and validate_state')
    DEFINITIONS[identifier]=definition
    AUDIO_PROCESSORS[identifier]=processor
    if default_state is not None:
        STATE_DEFAULTS[identifier]=default_state
        STATE_VALIDATORS[identifier]=validate_state


class GainProcessor:
    """Example plugin. A stereo processing node with gain, saturation and wet mix."""
    def __init__(self,sample_rate=48000):
        self.sr=sample_rate
    def process(self,n,controls,inputs):
        left=np.asarray(inputs.get('inL',np.zeros(n,np.float32)),np.float32)
        right=np.asarray(inputs.get('inR',np.zeros(n,np.float32)),np.float32)
        gain=float(controls['GAIN'])/100.0*2.0
        drive=float(controls['SATURATION'])/100*4.0
        wet=float(controls['MIX'])/100.0
        def shape(x):
            v=x*gain
            if drive>0.001:
                v=np.tanh(v*(1+drive))/np.tanh(1+drive)
            return np.asarray((1-wet)*x+wet*v,np.float32)
        return {'outL':shape(left),'outR':shape(right)}


def builtin_plugins():
    if 'gain' in DEFINITIONS:return
    register_audio_module('gain',{
        'title':'GAIN / SHAPE','desc':'Stereo utility processor','kind':'PROCESSOR',
        'code':'P / 004','w':490,'h':378,
        'ins':[Port('inL','in','audio'),Port('inR','in','audio')],
        'outs':[Port('outL','out','audio'),Port('outR','out','audio')],
        'controls':{'GAIN':50,'SATURATION':0,'MIX':100},'modes':[],
    },GainProcessor)

builtin_plugins()
