import copy,unittest
import numpy as np
from wirescape.plugins import AUDIO_PROCESSORS,GainProcessor
from wirescape.graph import AudioGraph
from test_graph import FakeSynth,P
from wirescape.schema import validate_patch

class TestGainModule(unittest.TestCase):
 def test_identity(self):
  gain=GainProcessor()
  x=np.ones(512,dtype=np.float32)*0.2
  y=gain.process(512,{'GAIN':50,'SATURATION':0,'MIX':100},{'inL':x,'inR':x})
  self.assertTrue(np.allclose(y['outL'],x))
 def test_stereo_chain(self):
  patch=copy.deepcopy(P)
  patch['modules'].append({'id':'gain-1','type':'gain','values':{'GAIN':10,'SATURATION':0,'MIX':100}})
  patch['connections']=[c for c in patch['connections'] if c['from'].startswith('mod-')]
  patch['connections'] += [{'from':'c-1:left','to':'gain-1:inL'},
     {'from':'c-1:right','to':'gain-1:inR'},
     {'from':'gain-1:outL','to':'io-1:inL'},
     {'from':'gain-1:outR','to':'io-1:inR'}]
  validate_patch(patch)
  g=AudioGraph(engine_factory=FakeSynth);g.load_patch(patch);g.keyboard('c-1',60,True)
  quieter=g.process(512)
  g.control('gain-1','GAIN',100);louder=g.process(512)
  self.assertGreater(float(louder.max()),float(quieter.max()))
 def test_reject_invalid_port(self):
  patch=copy.deepcopy(P);patch['modules'].append({'id':'gain-1','type':'gain'})
  patch['connections'].append({'from':'gain-1:bad','to':'io-1:inL'})
  from wirescape.schema import PatchError
  with self.assertRaises(PatchError):validate_patch(patch)
