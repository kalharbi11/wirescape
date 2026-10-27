"""Folder-plugin SDK acceptance tests: no hardware or Surge needed."""
import copy
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from wirescape import extensions
from wirescape.schema import DEFINITIONS, PatchError, validate_patch
from wirescape.graph import AudioGraph
from tests_sample import FakeEngine
from modules.sequencer.engine import default_pattern, validate_pattern, chord_notes
from modules.sequencer.plugin import register as register_sequencer


def seq_patch():
    return {'format':'wirescape.patch','version':3,'modules':[
        {'id':'sq1','type':'sequencer','module_state':default_pattern(),'values':{'RUN':100}},
        {'id':'ch1','type':'chaos'},
        {'id':'io1','type':'interface'}], 'connections':[
        {'from':'sq1:notes','to':'ch1:notes'},
        {'from':'sq1:mod','to':'ch1:cutoff'},
        {'from':'ch1:left','to':'io1:inL'},
        {'from':'ch1:right','to':'io1:inR'}]}

class FolderPluginTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if 'sequencer' not in DEFINITIONS:
            register_sequencer()  # purely in-process test registration; no persistent enabling

    def test_manifest_discovery_not_import(self):
        d=extensions.scan()
        self.assertIn('sequencer',d)
        self.assertEqual(d['sequencer']['api_version'],1)
        self.assertEqual(d['sequencer']['assets'],['editor.js','editor.css'])

    def test_module_state_validation(self):
        p=seq_patch()
        self.assertEqual(len(validate_patch(p)),3)
        bad=copy.deepcopy(p)
        bad['modules'][0]['module_state']['steps'][0]['quality']='unknown'
        with self.assertRaises(PatchError):validate_patch(bad)

    def test_legacy_sequence_migration_host(self):
        p=seq_patch();s=p['modules'][0];s['sequence']=s.pop('module_state')
        self.assertEqual(len(validate_patch(p)),3)
        g=AudioGraph(engine_factory=FakeEngine);g.load_patch(p);g.process()
        self.assertEqual(g.nodes['sq1'].plugin.pattern['length'],16)

    def test_notes_reach_chaos_and_output(self):
        g=AudioGraph(engine_factory=FakeEngine);g.load_patch(seq_patch())
        a=g.process()
        ch=g.nodes['ch1']
        self.assertEqual(set(ch.synth.notes),{48,51,55,58})
        self.assertGreater(float(a.max()),0)
        before=ch.synth.state['CUTOFF']
        for _ in range(20):g.process()
        self.assertNotEqual(ch.synth.state['CUTOFF'],before)

    def test_state_edit_and_disconnect_release(self):
        g=AudioGraph(engine_factory=FakeEngine);p=seq_patch();g.load_patch(p);g.process()
        next_pattern=default_pattern();next_pattern['steps'][0]['root']=7
        g.module_state('sq1',next_pattern);g.process()
        self.assertEqual(set(g.nodes['ch1'].synth.notes),{55,58,62,65})
        p['connections']=[c for c in p['connections'] if c['from']!='sq1:notes']
        g.load_patch(p);g.process()
        self.assertFalse(g.nodes['ch1'].synth.notes)

    def test_multiple_sources_ownership(self):
        p=seq_patch()
        p['modules'].insert(1,{'id':'sq2','type':'sequencer','module_state':default_pattern()})
        p['connections'].insert(1,{'from':'sq2:notes','to':'ch1:notes'})
        g=AudioGraph(engine_factory=FakeEngine);g.load_patch(p);g.process()
        self.assertEqual(set(g.nodes['ch1'].synth.notes),{48,51,55,58})
        p['connections']=[x for x in p['connections'] if x['from']!='sq1:notes']
        g.load_patch(p);g.process()
        self.assertEqual(set(g.nodes['ch1'].synth.notes),{48,51,55,58})
        g.panic();g.process()
        self.assertFalse(g.nodes['ch1'].synth.notes)

    def test_bad_note_payload_is_rejected(self):
        from wirescape.plugins import AUDIO_PROCESSORS
        p=seq_patch();g=AudioGraph(engine_factory=FakeEngine);g.load_patch(p)
        g.process()
        old=g.nodes['sq1'].plugin.process
        try:
            g.nodes['sq1'].plugin.process=lambda n,c,i:{'notes':[('C4',True)],'gate':False,'trig':False,'mod':0,'env':0}
            with self.assertRaises(RuntimeError):g.process()
        finally:g.nodes['sq1'].plugin.process=old

    def test_folder_not_enabled_by_manifest_alone(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'enabled.json'
            with patch.object(extensions,'ENABLED',path):
                self.assertNotIn('arbitrary_module',extensions.read_enabled())
                extensions.write_enabled({'sequencer'})
                self.assertEqual(extensions.read_enabled(),{'sequencer'})

    def test_ui_assets_only_from_enabled_registry(self):
        with self.assertRaises(FileNotFoundError):extensions.asset('untrusted','editor.js')

if __name__=='__main__':unittest.main()

class ModuleManagerWebsocketTests(unittest.IsolatedAsyncioTestCase):
    async def test_catalog_and_module_state_roundtrip(self):
        from aiohttp.test_utils import TestClient, TestServer
        from wirescape.server import Host
        from wirescape.graph import SilentSynth
        host=Host(engine_factory=SilentSynth)
        async with TestServer(host.make_app()) as srv:
            async with TestClient(srv) as client:
                sock=await client.ws_connect('/ws')
                hello=await sock.receive_json()
                self.assertEqual(hello['type'],'hello')
                self.assertIn('module_assets',hello['schema'])
                await sock.send_json({'type':'modules_scan'})
                resp=await sock.receive_json()
                self.assertEqual(resp['type'],'modules_catalog')
                self.assertIn('sequencer',{x['id'] for x in resp['modules']})
                await sock.send_json({'type':'patch','state':seq_patch()})
                ack=await sock.receive_json()
                self.assertEqual(ack['type'],'ack')
                new=default_pattern();new['length']=12
                await sock.send_json({'type':'module_state','id':'sq1','state':new})
                import asyncio
                await asyncio.sleep(0.02)
                host.graph.process()
                self.assertEqual(host.graph.nodes['sq1'].plugin.pattern['length'],12)
                await sock.close()
