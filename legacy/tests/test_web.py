import asyncio
import json
import unittest
from aiohttp.test_utils import AioHTTPTestCase, unittest_run_loop
from wirescape.server import Host
from wirescape.graph import SilentSynth
from test_graph import P

class TestServer(AioHTTPTestCase):
 async def get_application(self):
  self.host=Host(engine_factory=SilentSynth,error='Test-only silent synth')
  return self.host.make_app()
 async def test_health(self):
  response=await self.client.get('/health')
  self.assertEqual(response.status,200)
  data=await response.json()
  self.assertTrue({'chaos','modulator','interface','gain'}.issubset(set(data['modules'])))
 async def test_websocket(self):
  ws=await self.client.ws_connect('/ws')
  hello=await ws.receive_json();self.assertEqual(hello['type'],'hello')
  await ws.send_json({'type':'patch','state':P})
  ack=await ws.receive_json();self.assertEqual(ack['type'],'ack')
  await ws.send_json({'type':'note','id':'c-1','note':60,'on':True})
  await ws.send_json({'type':'control','id':'c-1','name':'CHAOS','value':91})
  await asyncio.sleep(.03);self.host.graph.process()
  self.assertEqual(self.host.graph.nodes['c-1'].controls['CHAOS'],91)
  await ws.close()
 async def test_bad_patch(self):
  ws=await self.client.ws_connect('/ws');await ws.receive_json()
  await ws.send_json({'type':'patch','state':{'modules':[],'connections':[{'from':'bad:1','to':'bad:2'}]}})
  error=await ws.receive_json();self.assertEqual(error['type'],'error')
  await ws.close()

class TestPresetWebSocket(AioHTTPTestCase):
 async def get_application(self):
  import tempfile
  from pathlib import Path
  from wirescape.preset_store import PresetStore
  self.tmp=tempfile.TemporaryDirectory()
  self.host=Host(engine_factory=SilentSynth,error='test-only')
  self.host.presets=PresetStore(Path(self.tmp.name))
  return self.host.make_app()

 async def tearDownAsync(self):
  await super().tearDownAsync()
  self.tmp.cleanup()

 async def test_save_load_preset_via_host(self):
  ws=await self.client.ws_connect('/ws');await ws.receive_json()
  await ws.send_json({'type':'patch','state':P})
  ack=await ws.receive_json();self.assertEqual(ack['type'],'ack')
  snapshot={'values':{'FOLD':74,'A_ATTACK':34},'advanced':{'BREAK':0.31},
            'raw':{'A|Filter 1 Type':10/35},'mode':1}
  await ws.send_json({'type':'preset_save','id':'c-1','name':'Created in Browser',
                      'state':snapshot})
  result=await ws.receive_json()
  self.assertEqual(result['type'],'preset_saved')
  self.assertEqual(result['entry']['kind'],'user')
  self.assertEqual(len(result['presets']),1)
  await ws.send_json({'type':'preset','id':'c-1','preset':result['entry']['id']})
  loaded=await ws.receive_json()
  self.assertEqual(loaded['type'],'preset_loaded')
  self.assertEqual(loaded['preset']['mode'],1)
  self.assertEqual(loaded['preset']['values']['A_ATTACK'],34)
  await ws.close()
