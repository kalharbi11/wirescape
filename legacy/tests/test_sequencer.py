"""Dedicated no-audio-device tests for the Wirescape chord sequencer."""
import copy
import unittest
from modules.sequencer.engine import (
    SequencerProcessor, chord_notes, default_pattern, validate_pattern, PatchError,
)
from wirescape.schema import validate_patch
from wirescape.graph import AudioGraph
from test_graph import FakeSynth


def patch(pattern=None):
    return {'format':'wirescape.patch', 'version':2,
            'modules':[{'id':'sq-1','type':'sequencer','sequence':pattern or default_pattern(),
                        'values':{'RUN':100,'BPM':50}},
                       {'id':'ch-1','type':'chaos'}, {'id':'io-1','type':'interface'}],
            'connections':[{'from':'sq-1:notes','to':'ch-1:notes'},
                           {'from':'sq-1:mod','to':'ch-1:cutoff'},
                           {'from':'sq-1:trig','to':'ch-1:gate'},
                           {'from':'ch-1:left','to':'io-1:inL'},
                           {'from':'ch-1:right','to':'io-1:inR'}]}


class TestSequencer(unittest.TestCase):
    def test_default_chord_and_inversion(self):
        p=default_pattern()
        self.assertEqual(chord_notes(p['steps'][0]),[48,51,55,58])
        p['steps'][0]['inversion']=1
        self.assertEqual(chord_notes(p['steps'][0]),[51,55,58,60])

    def test_sequence_validation(self):
        p=default_pattern()
        self.assertEqual(len(validate_pattern(p)['steps']),16)
        for val in [float('nan'),float('inf'),-1,99]:
            q=copy.deepcopy(p);q['steps'][0]['probability']=val
            with self.assertRaises(PatchError):validate_pattern(q)
        q=copy.deepcopy(p);q['steps'][0]['quality']='arbitrary_python'
        with self.assertRaises(PatchError):validate_patch(patch(q))
        q=copy.deepcopy(p);q['steps'][0]['duration_steps']=0
        with self.assertRaises(PatchError):validate_pattern(q)

    def test_default_notes_and_cv_to_chaos(self):
        g=AudioGraph(engine_factory=FakeSynth)
        g.load_patch(patch())
        y=g.process()
        ch=g.nodes['ch-1']
        # First chord starts immediately and routes as notes into the engine.
        for pitch in (48,51,55,58):self.assertIn(pitch,ch.synth.notes)
        self.assertGreater(float(y.max()),0)
        # The filter receives sequenced bipolar CV, not just knob updates.
        first=ch.synth.state['CUTOFF']
        for _ in range(20):g.process()
        self.assertNotEqual(ch.synth.state['CUTOFF'],first)

    def test_default_chord_held_through_intermediate_steps(self):
        g=AudioGraph(engine_factory=FakeSynth)
        p=patch();p['connections']=[c for c in p['connections'] if c['from']!='sq-1:trig']
        g.load_patch(p);g.process()
        for _ in range(24):g.process()  # past steps 1 and 2, before next chord
        self.assertEqual(set(g.nodes['ch-1'].synth.notes),{48,51,55,58})

    def test_recurrence_condition(self):
        seq=default_pattern()
        seq['length']=1
        seq['steps'][0]['every']=2
        seq['steps'][0]['duration_steps']=1
        seq['steps'][0]['gate']=0.4
        # Every second loop is silent; use rapid 240 BPM and 256 sample blocks.
        proc=SequencerProcessor()
        proc.configure(seq)
        controls={'RUN':100,'BPM':100,'SWING':0,'DEPTH':100,'DECAY':35}
        attacks=[]
        for i in range(200):
            out=proc.process(256,controls,{})
            if any(on for _,on in out['notes']):attacks.append(i)
        self.assertTrue(len(attacks)>1)
        self.assertTrue(all((a-b)>=20 for b,a in zip(attacks,attacks[1:])))

    def test_stop_and_restart_do_not_fast_forward(self):
        g=AudioGraph(engine_factory=FakeSynth)
        p=patch();p['connections']=[c for c in p['connections'] if c['from']!='sq-1:trig']
        g.load_patch(p);g.process()
        g.control('sq-1','RUN',0)
        for _ in range(200):g.process()
        self.assertEqual(g.nodes['ch-1'].synth.notes,[])
        g.control('sq-1','RUN',100)
        g.process()
        self.assertEqual(set(g.nodes['ch-1'].synth.notes),{48,51,55,58})

    def test_live_pattern_edit(self):
        g=AudioGraph(engine_factory=FakeSynth)
        p=patch();p['connections']=[c for c in p['connections'] if c['from']!='sq-1:trig']
        g.load_patch(p);g.process()
        replacement=default_pattern();replacement['steps'][0]['root']=7
        g.module_state('sq-1',replacement);g.process()
        self.assertEqual(set(g.nodes['ch-1'].synth.notes),{55,58,62,65})

    def test_disconnect_release_and_panic(self):
        g=AudioGraph(engine_factory=FakeSynth)
        p=patch();p['connections']=[c for c in p['connections'] if c['from']!='sq-1:trig']
        g.load_patch(p);g.process()
        p['connections']=[c for c in p['connections'] if c['from']!='sq-1:notes']
        g.load_patch(p);g.process()
        self.assertEqual(g.nodes['ch-1'].synth.notes,[])
        p['connections'].append({'from':'sq-1:notes','to':'ch-1:notes'})
        g.load_patch(p);g.process()
        g.panic();g.process()
        self.assertEqual(g.nodes['ch-1'].synth.notes,[])

    def test_external_clock_and_gate_reset(self):
        s=SequencerProcessor()
        s.mode=1
        s.configure(default_pattern())
        opts={'RUN':100,'BPM':50,'SWING':0,'DEPTH':100,'DECAY':35}
        a=s.process(512,opts,{'clock':False})
        self.assertEqual(a['notes'],[])
        b=s.process(512,opts,{'clock':True})
        self.assertEqual({e[0] for e in b['notes'] if e[1]}, {48,51,55,58})
        self.assertTrue(b['trig'])
        c=s.process(512,opts,{'clock':True})
        self.assertEqual(c['notes'],[])
        s.process(512,opts,{'clock':False})
        s.process(512,opts,{'clock':True})
        self.assertEqual(s.step,1)
        s.process(512,opts,{'clock':False,'reset':True})
        self.assertEqual(s.step,-1)

    def test_cv_gate_and_notes_have_strict_types(self):
        p=patch();self.assertEqual(len(validate_patch(p)),3)
        p['connections'].append({'from':'sq-1:mod','to':'ch-1:notes'})
        with self.assertRaises(PatchError):validate_patch(p)

    def test_reject_bad_sequence_update_without_mutation(self):
        g=AudioGraph(engine_factory=FakeSynth);g.load_patch(patch());g.process()
        bad=default_pattern();bad['steps'][0]['quality']='illegal'
        with self.assertRaises(PatchError):g.module_state('sq-1',bad)
        self.assertEqual(g.nodes['sq-1'].plugin.pattern['steps'][0]['quality'],'m7')


if __name__=='__main__':unittest.main()
