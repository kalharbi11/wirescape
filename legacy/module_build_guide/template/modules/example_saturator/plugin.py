"""API v1: author-owned module definition; host registers it dynamically."""
from wirescape.schema import Port
from wirescape.plugins import register_audio_module
from .engine import SaturatorProcessor

def register():
    register_audio_module('example_saturator',{
        'title':'EXAMPLE SATURATOR','desc':'Stereo saturation / example module',
        'kind':'PROCESSOR','code':'P / 101','w':520,'h':370,
        'ins':[Port('inL','in','audio'),Port('inR','in','audio')],
        'outs':[Port('outL','out','audio'),Port('outR','out','audio')],
        'controls':{'LEVEL':50,'DRIVE':20,'MIX':100},
        'modes':[],
    },SaturatorProcessor)
