"""Independent folder plugin: all registrations and state live here."""
from wirescape.schema import Port
from wirescape.plugins import register_audio_module
from .engine import SequencerProcessor, default_pattern, validate_pattern

def register():
    register_audio_module('sequencer',{
        'title':'CHORD / SEQUENCER', 'desc':'16-step chords + independent CV',
        'kind':'SEQUENCER', 'code':'S / 005', 'w':625, 'h':510,
        'ins':[Port('clock','in','clock'),Port('reset','in','gate')],
        'outs':[Port('notes','out','notes'),Port('gate','out','gate'),
                Port('trig','out','gate'),Port('mod','out','mod'),Port('env','out','mod')],
        'controls':{'RUN':100,'BPM':50,'SWING':0,'DEPTH':100,'DECAY':35},
        'modes':['INTERNAL','EXTERNAL'],
    }, SequencerProcessor, default_state=default_pattern, validate_state=validate_pattern)
