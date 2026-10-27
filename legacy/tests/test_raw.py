import unittest,json
from pathlib import Path
from wirescape.graph import AudioGraph,full_raw_keys
from test_graph import FakeSynth,P

class TestRaw(unittest.TestCase):
 def test_full_parameter_snapshot(self):
  self.assertGreater(len(full_raw_keys()),700)
  self.assertIn('S1:Osc 1 Pitch',full_raw_keys())
 def test_roundtrip_full_raw(self):
  graph=AudioGraph(engine_factory=FakeSynth)
  graph.load_patch(P)
  graph.raw('c-1','A','S1:Osc 1 Pitch',0.63)
  graph.process()
  self.assertEqual(graph.nodes['c-1'].raw_overrides['A','S1:Osc 1 Pitch'],0.63)
