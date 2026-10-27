"""Wirescape v0.3 panel mapping / control-priority regression tests.

No Surge build or audio interface needed. Run via unittest discovery.
"""
import copy
import math
import unittest
from wirescape.graph import AudioGraph
from wirescape.schema import DEFINITIONS
from wirescape.mapping import chaos_macros, FRONT_OWNS
from test_graph import FakeSynth, P
import modules.chaos.macros as M


class TestExpandedChaosPanel(unittest.TestCase):
    def test_all_30_controls_have_mappings(self):
        controls=DEFINITIONS['chaos']['controls']
        self.assertEqual(len(controls),30)
        self.assertEqual(set(controls),set(FRONT_OWNS))
        for mode in range(3):
            result=chaos_macros(controls,mode)
            for key,value in result.items():
                if key=='LFO_TARGET':
                    self.assertIn(value,('off','CUTOFF'))
                else:
                    self.assertIn(key,M.SPEC)
                    self.assertTrue(math.isfinite(float(value)),key)
                    lo,hi,_=M.SPEC[key]
                    self.assertGreaterEqual(value,lo-1e-6,key)
                    self.assertLessEqual(value,hi+1e-6,key)

    def test_defaults_are_non_destructive(self):
        result=chaos_macros(DEFINITIONS['chaos']['controls'])
        self.assertEqual(result['B_MUTE'],1)
        self.assertEqual(result['BREAK'],0)
        self.assertEqual(result['CUTOFF'],.75)
        self.assertEqual(result['CLUSTER'],2)
        self.assertEqual(result['ROUTE_MODE'],0)
        self.assertEqual(result['LFO_DEPTH'],0)
        self.assertEqual(result['LFO_TARGET'],'off')

    def test_parameters_make_real_macro_changes(self):
        defaults=DEFINITIONS['chaos']['controls']
        expected={'FOLD':'BREAK','CUTOFF':'CUTOFF','DETUNE':'DETUNE',
                  'CLUSTER':'CLUSTER','SUB':'SUB','FM':'FM_RATIO',
                  'ROUGH':'ROUGH_DEPTH','JITTER':'JITTER','DRIFT':'DRIFT',
                  'FALL':'FALL','COUPLE':'ROUTE_DEPTH','BODY':'BODY_MIX',
                  'TONE':'BODY_TONE','FEEDBACK':'BODY_FEED',
                  'RATE':'LFO_RATE','MOTION':'LFO_DEPTH','TAPE':'TAPE',
                  'SPACE':'SPACE_MIX','LOOP':'LOOP_MIX'}
        base=chaos_macros(defaults)
        for control,macro in expected.items():
            variant={**defaults,control:100 if defaults[control]!=100 else 0}
            mapped=chaos_macros(variant)
            self.assertNotEqual(mapped[macro],base[macro],control)

    def test_modes_are_actual_routing_scenes(self):
        base=DEFINITIONS['chaos']['controls']
        self.assertEqual(chaos_macros({**base,'COUPLE':75},0)['ROUTE_MODE'],1)
        self.assertEqual(chaos_macros({**base,'COUPLE':75},1)['ROUTE_MODE'],2)
        self.assertEqual(chaos_macros({**base,'COUPLE':75},2)['ROUTE_MODE'],1)
        self.assertEqual(chaos_macros({**base,'COUPLE':0},0)['ROUTE_MODE'],0)

    def test_motion_assigns_cutoff_lfo(self):
        base=DEFINITIONS['chaos']['controls']
        a=chaos_macros({**base,'MOTION':80})
        self.assertEqual(a['LFO_TARGET'],'CUTOFF')
        self.assertGreater(a['LFO_DEPTH'],0)

    def test_macro_override_is_cleared_when_front_control_is_touched(self):
        patch=copy.deepcopy(P)
        patch['modules'][1]['advanced']={'BREAK':0.9,'CUTOFF':0.1,'MASTER_DB':-12}
        g=AudioGraph(engine_factory=FakeSynth)
        g.load_patch(patch)
        g.process()
        self.assertAlmostEqual(g.nodes['c-1'].synth.state['BREAK'],.9)
        g.control('c-1','FOLD',55)
        g.process()
        state=g.nodes['c-1'].synth.state
        self.assertAlmostEqual(state['BREAK'],.55)
        self.assertAlmostEqual(state['CUTOFF'],.1)
        self.assertEqual(state['MASTER_DB'],-12)
        self.assertNotIn('BREAK',g.nodes['c-1'].advanced)
        self.assertIn('CUTOFF',g.nodes['c-1'].advanced)

    def test_exposed_chaos_parameter_changes_actual_dsp_state(self):
        patch=copy.deepcopy(P)
        patch['modules'][1]['values']={'FOLD':0,'CUTOFF':75,'MOTION':0}
        g=AudioGraph(engine_factory=FakeSynth)
        g.load_patch(patch)
        g.process()
        self.assertEqual(g.nodes['c-1'].synth.state['BREAK'],0)
        g.control('c-1','FOLD',90)
        g.control('c-1','MOTION',77)
        g.process()
        self.assertEqual(g.nodes['c-1'].synth.state['BREAK'],.9)
        self.assertEqual(g.nodes['c-1'].synth.state['LFO_TARGET'],'CUTOFF')
        self.assertAlmostEqual(g.nodes['c-1'].synth.state['LFO_DEPTH'],.77)

    def test_legacy_six_knob_patches_still_load(self):
        patch=copy.deepcopy(P)
        patch['modules'][1]['values']={'MORPH':27,'CHAOS':33,'FOLD':8,'PITCH':54,'DETUNE':20,'SPACE':37}
        g=AudioGraph(engine_factory=FakeSynth)
        g.load_patch(patch)
        g.process()
        node=g.nodes['c-1']
        self.assertEqual(node.controls['MORPH'],27)
        self.assertEqual(node.controls['CUTOFF'],75)
        self.assertEqual(node.controls['COUPLE'],0)
        self.assertAlmostEqual(node.synth.state['SPACE_MIX'],.37*.65)

if __name__=='__main__':unittest.main()
