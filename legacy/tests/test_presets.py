"""Preset round-trips, filesystem safety, mode compatibility, and schema enum type metadata."""
import json
import tempfile
import unittest
from pathlib import Path
from wirescape.preset_store import PresetStore, PresetError
from wirescape.server import schema_payload, ROOT
from wirescape.mapping import chaos_macros
from wirescape.schema import DEFINITIONS

class TestPresetStore(unittest.TestCase):
 def test_save_roundtrip_complete_sound(self):
  with tempfile.TemporaryDirectory() as d:
   store=PresetStore(Path(d))
   state={'values':{'FOLD':61,'A_ATTACK':48,'B_SUSTAIN':82},'advanced':{'BREAK':.43,'VOICES':4},
          'raw':{'A|Filter 1 Type':10/35,'B|Osc 1 Type':1/11},'mode':1}
   record=store.save('Industrial Disintegration',state)
   self.assertEqual(record['kind'],'user')
   loaded=store.load(record['id'])
   self.assertEqual(loaded['values']['A_ATTACK'],48)
   self.assertAlmostEqual(loaded['raw']['A|Filter 1 Type'],10/35)
   self.assertEqual(loaded['mode'],1)
   self.assertEqual(len(store.list()),1)
   duplicate=store.save('Industrial Disintegration',state)
   self.assertNotEqual(record['id'],duplicate['id'])
 def test_rejects_malicious_and_invalid_data(self):
  with tempfile.TemporaryDirectory() as d:
   p=PresetStore(Path(d))
   with self.assertRaises(PresetError):p.load('../private')
   with self.assertRaises(PresetError):p.save('!!!',{'values':{}})
   with self.assertRaises(PresetError):p.save('Bad raw',{'raw':{'B|something bogus':0.3}})
   with self.assertRaises(PresetError):p.save('NaN',{'values':{'FOLD':float('nan')}})
   with self.assertRaises(PresetError):p.save('Bad enum',{'mode':99})
 def test_factory_and_starter_modes(self):
  p=PresetStore(ROOT)
  self.assertGreaterEqual(len(p.list()),8)
  scene=p.load('factory/7_steel_interference')
  self.assertEqual(scene['mode'],1)
  self.assertEqual(scene['values']['FM'],75)
  legacy=p.load('factory/1_clean_to_scream')
  self.assertIn('BREAK',legacy['advanced'])

class TestTypedSchema(unittest.TestCase):
 def test_enum_raw_metadata_and_labels(self):
  schema=schema_payload()
  self.assertEqual(schema['raw_metadata']['Filter 1 Type']['type'],'int')
  self.assertEqual(schema['raw_metadata']['Filter 1 Type']['max'],35)
  self.assertEqual(schema['raw_metadata']['Filter Configuration']['type'],'int')
  self.assertEqual(schema['raw_metadata']['Osc 1 Mute']['type'],'bool')
  self.assertEqual(schema['raw_metadata']['Amp EG Attack']['type'],'float')
  self.assertIn('A_ATTACK',schema['front_descriptions'])
  self.assertIn('MORPH',schema['front_descriptions'])
 def test_modes_not_dormant_at_default_values(self):
  default=DEFINITIONS['chaos']['controls']
  cluster=chaos_macros(default,0)
  fm=chaos_macros(default,1)
  noise=chaos_macros(default,2)
  self.assertGreaterEqual(cluster['CLUSTER'],2)
  self.assertEqual(fm['ROUTE_MODE'],2)
  self.assertGreater(fm['ROUTE_DEPTH'],0)
  self.assertGreater(fm['BREAK'],0)
  self.assertGreater(fm['FM_RATIO'],0)
  self.assertEqual(fm['B_MUTE'],0)
  self.assertNotEqual(fm['ROUTE_MODE'],noise['ROUTE_MODE'])
