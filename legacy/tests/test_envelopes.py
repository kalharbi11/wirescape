import copy
import queue
import unittest
from test_graph import P, FakeSynth
from wirescape.graph import AudioGraph

class EnvelopeFake(FakeSynth):
 def __init__(self,sr):
  super().__init__(sr)
  self.q=queue.SimpleQueue()

def readq(q):
 out=[]
 while not q.empty():out.append(q.get_nowait())
 return out

class TestIndependentADSR(unittest.TestCase):
 def test_a_and_b_receive_distinct_normalized_envelopes(self):
  g=AudioGraph(engine_factory=EnvelopeFake)
  g.load_patch(P)
  g.control('c-1','A_ATTACK',65)
  g.control('c-1','B_ATTACK',33)
  g.control('c-1','A_RELEASE',88)
  g.process()
  commands=readq(g.nodes['c-1'].synth.q)
  self.assertIn(('raw','A',('Amp EG Attack',.65)),commands)
  self.assertIn(('raw','B',('Amp EG Attack',.33)),commands)
  self.assertIn(('raw','A',('Amp EG Release',.88)),commands)
  self.assertIn(('raw','B',('Amp EG Sustain',1.0)),commands)
 def test_raw_override_beats_adsr_panel_then_clearing_restores_panel(self):
  g=AudioGraph(engine_factory=EnvelopeFake)
  patch=copy.deepcopy(P)
  patch['modules'][1]['raw']={'A|Amp EG Attack':.81}
  g.load_patch(patch)
  g.process()
  commands=readq(g.nodes['c-1'].synth.q)
  self.assertIn(('raw','A',('Amp EG Attack',.81)),commands)
  self.assertNotIn(('raw','A',('Amp EG Attack',.15)),commands)
  g.raw('c-1','A','Amp EG Attack',0,clear=True)
  g.process()
  commands=readq(g.nodes['c-1'].synth.q)
  self.assertIn(('raw_clear','A','Amp EG Attack'),commands)
  self.assertIn(('raw','A',('Amp EG Attack',.15)),commands)
