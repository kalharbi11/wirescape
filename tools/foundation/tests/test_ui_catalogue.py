# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('gen_ui_catalogue', ROOT / 'tools/gen_ui_catalogue.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class UiCatalogueTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for pattern in ('native/modules2/*/module.json', 'native/modules2/*/dsp.h',
                        'native/sdk/examples/*/module.json', 'native/sdk/examples/*/dsp.h',
                        'apps/studio/public/*catalogue*.json', 'apps/studio/public/wirescape_modules.json',
                        'tools/gen_ui_catalogue.py'):
            for path in ROOT.glob(pattern):
                target = self.root / path.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, target)

    def check(self):
        return subprocess.run([sys.executable, str(self.root / 'tools/gen_ui_catalogue.py'), '--check'],
                              capture_output=True, text=True)

    def test_check_detects_stale_output_and_descriptor_version(self):
        self.assertEqual(self.check().returncode, 0)
        path = self.root / 'native/modules2/host_keyboard/dsp.h'
        path.write_text(path.read_text().replace('"Keyboard", 1', '"Keyboard", 7'))
        self.assertNotEqual(self.check().returncode, 0)
        modules = json.loads(generator.generate(self.root))['modules']
        self.assertEqual(next(m for m in modules if m['id'] == 'host.keyboard')['version'], 7)

    def test_identity_mismatch_is_refused_without_rewriting_files(self):
        path = self.root / 'native/modules2/vco/module.json'
        data = json.loads(path.read_text())
        data['inputs']['renamed'] = data['inputs'].pop('pitch')
        path.write_text(json.dumps(data))
        target = self.root / 'apps/studio/public/wirescape_modules.json'
        before = target.read_bytes()
        with self.assertRaisesRegex(ValueError, 'ID mismatch'):
            generator.generate(self.root)
        self.assertEqual(target.read_bytes(), before)

    def test_native_port_kind_wins_over_presentation(self):
        path = self.root / 'native/modules2/vco/module.json'
        data = json.loads(path.read_text())
        data['inputs']['pitch'] = 'event'
        path.write_text(json.dumps(data))
        module = next(m for m in json.loads(generator.generate(self.root))['modules'] if m['id'] == data['id'])
        self.assertEqual(next(p for p in module['ports'] if p['id'] == 'pitch')['kind'], 'event')

    def test_conditional_visibility_and_unknown_presentation_keys_survive(self):
        path = self.root / 'apps/studio/public/catalogue-presentation.json'
        data = json.loads(path.read_text())
        param = data['modules'][0]['parameters'][0]
        param['visibleWhen'] = {'param': 'mode', 'values': [6, 7], 'future': True}
        param['futurePresentation'] = {'nested': [1, 2]}
        path.write_text(json.dumps(data))
        generated = json.loads(generator.generate(self.root))['modules'][0]['parameters'][0]
        self.assertEqual(generated, param)
