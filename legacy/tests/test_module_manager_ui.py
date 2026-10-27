"""Regression tests for module manager visibility and bundled sequencer discovery.

These tests verify the browser source contract and live host registry. They do not
replace an interactive browser test or physical audio testing.
"""
from pathlib import Path
from unittest import TestCase, IsolatedAsyncioTestCase
from unittest.mock import patch
from tempfile import TemporaryDirectory

from wirescape import extensions

ROOT = Path(__file__).resolve().parents[1]


class ModuleManagerMarkupTests(TestCase):
    def test_manager_opens_with_visible_class_not_display_only(self):
        html = (ROOT / 'web/index.html').read_text(encoding='utf8')
        js = (ROOT / 'web/client.js').read_text(encoding='utf8')
        self.assertIn('id="moduleManager"', html)
        self.assertNotIn('id="moduleManager" class="device-panel" aria-label="Installed Wirescape modules" style="display:none"', html)
        self.assertIn("panel.classList.toggle('visible',!!visible)", js)
        self.assertIn('setModuleManagerVisible(true)', js)
        self.assertIn('setModuleManagerVisible(false)', js)
        self.assertIn('.device-panel.visible{transform:translateX(0)}', html)

    def test_fresh_install_preapproves_only_bundled_sequencer(self):
        with TemporaryDirectory() as d:
            enabled = Path(d) / 'enabled_modules.json'
            with patch.object(extensions, 'ENABLED', enabled):
                self.assertEqual(extensions.read_enabled(), {'sequencer'})
                extensions.write_enabled(set())
                self.assertEqual(extensions.read_enabled(), set())
                extensions.write_enabled({'sequencer'})
                self.assertEqual(extensions.read_enabled(), {'sequencer'})


class ModuleManagerHostTests(IsolatedAsyncioTestCase):
    async def test_new_host_serves_enabled_sequencer_and_assets(self):
        from aiohttp.test_utils import TestClient, TestServer
        from wirescape.server import Host
        from wirescape.graph import SilentSynth
        with TemporaryDirectory() as d:
            with patch.object(extensions, 'ENABLED', Path(d) / 'enabled_modules.json'):
                host = Host(engine_factory=SilentSynth)
                async with TestServer(host.make_app()) as srv:
                    async with TestClient(srv) as client:
                        socket = await client.ws_connect('/ws')
                        hello = await socket.receive_json()
                        self.assertEqual(hello['type'], 'hello')
                        self.assertIn('sequencer', hello['schema']['plugin_definitions'])
                        self.assertIn('sequencer', hello['schema']['module_assets'])
                        self.assertTrue(any(m['id']=='sequencer' and m['active'] for m in hello['schema']['installed_modules']))
                        response = await client.get('/module-assets/sequencer/editor.js')
                        self.assertEqual(response.status, 200)
                        self.assertIn('WirescapeModuleUI.sequencer', await response.text())
                        await socket.send_json({'type': 'modules_scan'})
                        catalog = await socket.receive_json()
                        self.assertEqual(catalog['type'], 'modules_catalog')
                        self.assertTrue(any(m['id']=='sequencer' and m['active'] for m in catalog['modules']))
                        await socket.close()
