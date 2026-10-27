"""Run with: python -m unittest discover -s tests -v (no audio hardware needed)."""
import copy
import json
import math
import unittest
import numpy as np
from wirescape.schema import validate_patch, PatchError,DEFINITIONS
from wirescape.graph import AudioGraph
from wirescape.mapping import chaos_macros
from wirescape.modulation import Modulator

class FakeSynth:
    count=0
    def __init__(self,sr):
        type(self).count+=1
        self.sr=sr
        self.state={}
        self.notes=[]
        self.last_input=None
        self.calls=0
    def set_many(self,kv):self.state.update(kv)
    def note_on(self,n):self.notes.append(n)
    def note_off(self,n):
        if n in self.notes:self.notes.remove(n)
    def panic(self):self.notes=[]
    def render(self,n,input_block=None):
        self.calls+=1
        self.last_input=input_block
        # mock sound has constant DC deliberately: host must still route/mix/limit.
        amplitude=(0.2+0.2*float(self.state.get('BREAK',0))) if self.notes else 0.0
        return np.full((2,n),amplitude,np.float32)

P={
 'format':'wirescape.patch','version':1,
 'modules':[{'id':'mod-1','type':'modulator','values':{'RATE':60,'DEPTH':100},'mode':0},
            {'id':'c-1','type':'chaos','values':{'MORPH':50,'CHAOS':30,'FOLD':30,'PITCH':50,'DETUNE':30,'SPACE':30},'mode':0},
            {'id':'io-1','type':'interface','values':{'OUTPUT LEVEL':71},'mode':0}],
 'connections':[{'from':'mod-1:modA','to':'c-1:chaos'},
                {'from':'c-1:left','to':'io-1:inL'},
                {'from':'c-1:right','to':'io-1:inR'}],
}

class TestSchema(unittest.TestCase):
 def test_good(self):self.assertEqual(set(validate_patch(P)),{'c-1','mod-1','io-1'})
 def test_missing_port(self):
  p=copy.deepcopy(P);p['connections'][0]['to']='c-1:not_port'
  with self.assertRaises(PatchError):validate_patch(p)
 def test_wrong_signal(self):
  p=copy.deepcopy(P);p['connections'][0]['to']='c-1:audio'
  with self.assertRaises(PatchError):validate_patch(p)
 def test_cycle(self):
  p=copy.deepcopy(P)
  p['connections'] += [{'from':'c-1:mod','to':'mod-1:rate'}]
  with self.assertRaisesRegex(PatchError,'cycle'):validate_patch(p)
 def test_many_to_one(self):
  p=copy.deepcopy(P);p['modules'].append({'id':'c-2','type':'chaos'})
  p['connections'].append({'from':'c-2:left','to':'io-1:inL'})
  validate_patch(p)
 def test_no_duplicates(self):
  p=copy.deepcopy(P);p['connections'].append(p['connections'][0])
  with self.assertRaises(PatchError):validate_patch(p)

class TestGraph(unittest.TestCase):
 def setUp(self):self.g=AudioGraph(engine_factory=FakeSynth)
 def test_audible_routing_and_notes(self):
  self.g.load_patch(P);self.g.keyboard('c-1',60,True)
  y=self.g.process(512)
  self.assertEqual(y.shape,(2,512));self.assertTrue(np.max(y)>0)
  self.assertTrue(np.isfinite(y).all())
  self.g.keyboard('c-1',60,False)
  self.assertAlmostEqual(float(self.g.process().max()),0,places=6)
 def test_modulation_changes(self):
  self.g.load_patch(P);self.g.keyboard('c-1',60,True)
  self.g.process();node=self.g.nodes['c-1'];v1=node.synth.state['JITTER']
  for _ in range(50):self.g.process()
  v2=node.synth.state['JITTER']
  self.assertNotAlmostEqual(v1,v2)
 def test_several_synths_one_stereo_output(self):
  p=copy.deepcopy(P);p['modules'].append({'id':'c-2','type':'chaos'})
  p['connections'] += [{'from':'c-2:left','to':'io-1:inL'}, {'from':'c-2:right','to':'io-1:inR'}]
  self.g.load_patch(p);self.g.keyboard('c-1',60,True);self.g.keyboard('c-2',64,True)
  y=self.g.process();self.assertTrue(0<np.max(y)<=1.0)
  self.assertEqual(len(self.g.nodes),4)
 def test_hot_swap_reuse(self):
  self.g.load_patch(P);self.g.process();old=self.g.nodes['c-1'].synth
  p=copy.deepcopy(P);p['connections']=p['connections'][:-1]
  self.g.load_patch(p);self.g.process();self.assertIs(self.g.nodes['c-1'].synth,old)
 def test_macro_override_and_reset(self):
  self.g.load_patch(P);self.g.advanced('c-1','JITTER',0.99);self.g.process()
  self.assertAlmostEqual(self.g.nodes['c-1'].synth.state['JITTER'],0.99)
  self.g.advanced('c-1','JITTER',0,clear=True);self.g.process()
  self.assertNotAlmostEqual(self.g.nodes['c-1'].synth.state['JITTER'],0.99)
 def test_patch_roundtrip_raw(self):
  p=copy.deepcopy(P);p['modules'][1]['advanced']={'CUTOFF':0.37};p['modules'][1]['raw']={'A|Osc 1 Type':0.3}
  self.g.load_patch(p);self.g.process()
  self.assertAlmostEqual(self.g.nodes['c-1'].advanced['CUTOFF'],0.37)
  self.assertAlmostEqual(self.g.nodes['c-1'].raw_overrides['A','Osc 1 Type'],0.3)
 def test_no_output_without_io(self):
  p=copy.deepcopy(P);p['modules']=p['modules'][:2];p['connections']=p['connections'][:1]
  self.g.load_patch(p);self.g.keyboard('c-1',60,True)
  self.assertTrue(np.all(self.g.process()==0))
 def test_input_device_not_sounding_by_itself(self):
  self.g.load_patch(P)
  y=self.g.process(input_audio=np.ones((2,512),np.float32))
  self.assertEqual(float(y.max()),0.0)
 def test_panic(self):
  self.g.load_patch(P);self.g.keyboard('c-1',60,True);self.g.process()
  self.g.panic();self.assertEqual(float(self.g.process().max()),0.0)
 def test_gain_clipping(self):
  p=copy.deepcopy(P);p['modules'].append({'id':'c-2','type':'chaos'});p['connections'] += [{'from':'c-2:left','to':'io-1:inL'},{'from':'c-2:right','to':'io-1:inR'}]
  self.g.load_patch(p);self.g.keyboard('c-1',60,True);self.g.keyboard('c-2',60,True)
  self.g.control('io-1','OUTPUT LEVEL',100);y=self.g.process()
  self.assertTrue(np.max(y)<=1.00001)
 def test_performance_proxy(self):
  self.g.load_patch(P);self.g.keyboard('c-1',60,True)
  import time
  t=time.perf_counter()
  for _ in range(150):self.g.process()
  ms=(time.perf_counter()-t)*1000/150
  self.assertLess(ms,10.6667, f'Mock DSP {ms:.2f}ms exceeded budget')

class TestControls(unittest.TestCase):
 def test_mapping_limits(self):
  for mode in range(3):
   values=chaos_macros({'MORPH':50,'CHAOS':50,'FOLD':50,'PITCH':50,'DETUNE':50,'SPACE':50},mode)
   self.assertAlmostEqual(values['OFFSET'],0)
   self.assertTrue(values['B_MUTE']==0)
 def test_modulator(self):
  m=Modulator()
  values=[m.process(512,{'RATE':50,'DEPTH':100,'DRIFT':30,'SHAPE':50,'RANDOM':30},0)['modA'] for _ in range(100)]
  self.assertGreater(max(values)-min(values),0.1)

if __name__=='__main__':unittest.main()

class TestGateAndMonitoring(unittest.TestCase):
 def test_gate_cable_triggers_fixed_note(self):
  p=copy.deepcopy(P)
  p['connections'].append({'from':'mod-1:gate','to':'c-1:gate'})
  g=AudioGraph(engine_factory=FakeSynth)
  g.load_patch(p)
  active=False
  for _ in range(120):
   g.process()
   if 48 in g.nodes['c-1'].synth.notes:
    active=True
    break
  self.assertTrue(active)
 def test_monitor_off_by_default_and_on_by_command(self):
  g=AudioGraph(engine_factory=FakeSynth);g.load_patch(P)
  audio=np.ones((2,512),np.float32)*.1
  self.assertEqual(float(g.process(input_audio=audio).max()),0)
  g.monitor('io-1',True)
  self.assertGreater(float(g.process(input_audio=audio).max()),0)

class TestImportKeepsRuntime(unittest.TestCase):
 def test_import_updates_existing_node_values(self):
  g=AudioGraph(engine_factory=FakeSynth);g.load_patch(P);g.process()
  old=g.nodes['c-1'].synth
  changed=copy.deepcopy(P)
  changed['modules'][1]['values']['CHAOS']=95
  changed['modules'][1]['mode']=1
  g.load_patch(changed);g.process()
  self.assertIs(g.nodes['c-1'].synth,old)
  self.assertEqual(g.nodes['c-1'].controls['CHAOS'],95)
  self.assertEqual(g.nodes['c-1'].mode,1)

class TestLegacyVisualImport(unittest.TestCase):
 def test_visual_only_monitor_does_not_enable_physical_monitor(self):
  p=copy.deepcopy(P)
  p['format']='wirescape.visual.study'
  p['modules'][-1]['monitor']=True
  g=AudioGraph(engine_factory=FakeSynth);g.load_patch(p);g.process()
  self.assertFalse(g.nodes['io-1'].monitor)
