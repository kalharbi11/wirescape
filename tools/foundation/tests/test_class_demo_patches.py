# SPDX-License-Identifier: GPL-3.0-only
"""Class-demo contract checks and device-free ten-second native renders."""
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import wave

ROOT = Path(__file__).resolve().parents[3]
CLI = None
if __name__ == '__main__' and len(sys.argv) > 1 and not sys.argv[1].startswith('-'):
    CLI = sys.argv.pop(1)
PATCHES = sorted((ROOT / 'native/examples').glob('class_demo_*.json'))


class ClassDemoPatches(unittest.TestCase):
    def test_patch_contracts(self):
        self.assertEqual(len(PATCHES), 3)
        manifests = {m['id']: m for p in (ROOT / 'native/modules2').glob('*/module.json')
                     for m in [json.loads(p.read_text())]}
        for path in PATCHES:
            with self.subTest(patch=path.name):
                doc = json.loads(path.read_text())
                self.assertEqual((doc['format'], doc['version']), ('wirescape.native.patch', 2))
                self.assertEqual(doc['devices'], [])
                self.assertEqual(doc['settings']['sample_rate'], 48000)
                self.assertEqual(doc['settings']['max_block_frames'], 256)
                nodes = {m['id']: m for m in doc['modules']}
                self.assertEqual(len(nodes), len(doc['modules']))
                for node in nodes.values():
                    manifest = manifests[node['module']]
                    self.assertLessEqual(node['parameters'].keys(), manifest.get('parameters', {}).keys())
                    self.assertTrue(all(math.isfinite(v) for v in node['parameters'].values()))
                cc = next(m for m in nodes.values() if m['module'] == 'midi.cc_to_cv')
                self.assertEqual([r['controller'] for r in cc['config']['rows']], list(range(21, 29)))
                self.assertTrue(all(r['channel'] == 0 for r in cc['config']['rows']))
                used = set()
                for cable in doc['connections']:
                    src, dst = cable['from'], cable['to']
                    self.assertIn(src['port'], manifests[nodes[src['module']]['module']]['outputs'])
                    target = manifests[nodes[dst['module']]['module']]
                    key = 'parameters' if cable['kind'] == 'modulation' else 'inputs'
                    self.assertIn(dst['port'], target.get(key, {}))
                    if cable['kind'] == 'modulation':
                        self.assertTrue(math.isfinite(cable['amount']) and abs(cable['amount']) <= 1)
                    if src['module'] == cc['id']:
                        used.add(src['port'])
                self.assertEqual(used, {f'cc{i}' for i in range(1, 9)})
                self.assertEqual({r['bus_channel'] for r in doc['outputs']}, {0, 1})

    @unittest.skipUnless(CLI, 'native CLI supplied by CTest for offline render acceptance')
    def test_offline_renders(self):
        with tempfile.TemporaryDirectory(prefix='class-demo-') as work:
            for path in PATCHES:
                with self.subTest(patch=path.name):
                    output = Path(work) / (path.stem + '.wav')
                    result = subprocess.run([CLI, 'play', str(path), '--seconds', '10',
                                             '--rate', '48000', '--block', '256',
                                             '--output', str(output)],
                                            capture_output=True, text=True, timeout=60)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    report = json.loads(result.stdout)
                    self.assertTrue(report['offline'])
                    self.assertEqual(report['loader'], 'runtime_patch')
                    self.assertEqual(report['frames'], 480000)
                    self.assertEqual((report['rate'], report['block']), (48000, 256))
                    self.assertTrue(all(math.isfinite(report[k]) for k in ('peak', 'rms')))
                    with wave.open(str(output), 'rb') as wav:
                        self.assertEqual((wav.getframerate(), wav.getnframes()), (48000, 480000))
                        self.assertEqual((wav.getnchannels(), wav.getsampwidth()), (2, 2))
                        self.assertEqual(len(wav.readframes(480001)), 480000 * 4)


if __name__ == '__main__':
    unittest.main()
