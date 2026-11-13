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
        path.write_text(path.read_text(encoding='utf-8').replace('"Keyboard", 1', '"Keyboard", 7'), encoding='utf-8')
        self.assertNotEqual(self.check().returncode, 0)
        modules = json.loads(generator.generate(self.root))['modules']
        self.assertEqual(next(m for m in modules if m['id'] == 'host.keyboard')['version'], 7)

    def test_identity_mismatch_is_refused_without_rewriting_files(self):
        path = self.root / 'native/modules2/vco/module.json'
        data = json.loads(path.read_text(encoding='utf-8'))
        data['inputs']['renamed'] = data['inputs'].pop('pitch')
        path.write_text(json.dumps(data), encoding='utf-8')
        target = self.root / 'apps/studio/public/wirescape_modules.json'
        before = target.read_bytes()
        with self.assertRaisesRegex(ValueError, 'ID mismatch'):
            generator.generate(self.root)
        self.assertEqual(target.read_bytes(), before)

    def test_native_port_kind_wins_over_presentation(self):
        path = self.root / 'native/modules2/vco/module.json'
        data = json.loads(path.read_text(encoding='utf-8'))
        data['inputs']['pitch'] = 'event'
        path.write_text(json.dumps(data), encoding='utf-8')
        module = next(m for m in json.loads(generator.generate(self.root))['modules'] if m['id'] == data['id'])
        self.assertEqual(next(p for p in module['ports'] if p['id'] == 'pitch')['kind'], 'event')

    def test_conditional_visibility_and_unknown_presentation_keys_survive(self):
        path = self.root / 'apps/studio/public/catalogue-presentation.json'
        data = json.loads(path.read_text(encoding='utf-8'))
        param = data['modules'][0]['parameters'][0]
        param['visibleWhen'] = {'param': 'mode', 'values': [6, 7], 'future': True}
        param['futurePresentation'] = {'nested': [1, 2]}
        path.write_text(json.dumps(data), encoding='utf-8')
        generated = json.loads(generator.generate(self.root))['modules'][0]['parameters'][0]
        self.assertEqual(generated, param)


def known_module_ids(path=ROOT / 'tools/foundation/fixtures/known_module_ids.txt'):
    lines = (line.strip() for line in path.read_text(encoding='utf-8').splitlines())
    return [line for line in lines if line and not line.startswith('#')]


class KnownModuleIdTests(unittest.TestCase):
    """Adding a module touches no test; removing or renaming a known ID fails (#687)."""

    def test_generated_catalogue_is_a_superset_of_known_ids(self):
        known = known_module_ids()
        self.assertEqual(len(known), len(set(known)), 'duplicate known module ID')
        modules = json.loads((ROOT / 'apps/studio/public/wirescape_modules.json').read_text(encoding='utf-8'))['modules']
        ids = [module['id'] for module in modules]
        self.assertEqual(len(ids), len(set(ids)), 'duplicate module ID in the generated catalogue')
        self.assertEqual(sorted(set(known) - set(ids)), [], 'known module IDs missing from the catalogue')

    def test_missing_known_id_is_reported(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'known.txt'
            path.write_text('# header\nessentials.vco\nexample.removed\n', encoding='utf-8')
            known = known_module_ids(path)
        self.assertEqual(known, ['essentials.vco', 'example.removed'])
        modules = json.loads((ROOT / 'apps/studio/public/wirescape_modules.json').read_text(encoding='utf-8'))['modules']
        self.assertEqual(sorted(set(known) - {module['id'] for module in modules}), ['example.removed'])


class ChaosCatalogueContractTests(unittest.TestCase):
    """Keep native CHAOS IDs, generated descriptors, and its Studio panel in sync."""

    ORIGINAL_PARAMS = (
        "shape_a", "shape_b", "detune", "octave_a", "octave_b", "level_a",
        "level_b", "pan_a", "pan_b", "route", "depth", "body_tune",
        "feedback", "damping", "body_mix", "roughness", "rough_rate", "drift",
        "jitter", "fall", "lfo_rate", "lfo_depth", "lfo_target", "lfo_self",
        "cutoff", "level",
    )
    SHAPES = ("Sine", "Triangle", "Sample & hold", "Smooth random")

    @classmethod
    def problems(cls, manifest, catalogue, panel):
        problems = []
        expected = (*cls.ORIGINAL_PARAMS, "lfo_shape")
        ids = manifest.get("parameters", {})
        if manifest.get("id") != "essentials.chaos_lite":
            problems.append("native module ID changed")
        if set(ids) != set(expected):
            problems.append("native parameter IDs changed")
        if any(ids.get(name) != index for index, name in enumerate(expected)):
            problems.append("native parameter slot changed")

        modules = [m for m in catalogue.get("modules", [])
                   if m.get("id") == "essentials.chaos_lite"]
        if len(modules) != 1:
            return problems + ["generated CHAOS descriptor missing or duplicated"]
        generated = modules[0]
        # Compare names to their source; don't freeze a display name in this test.
        if generated.get("name") != manifest.get("name"):
            problems.append("generated descriptor name differs from native manifest")
        if generated.get("legacy_parameters") != ids:
            problems.append("generated parameter slots differ from native manifest")
        parameters = generated.get("parameters", [])
        if [param.get("id") for param in parameters] != list(expected):
            problems.append("generated parameter ordering changed")
        shapes = [param for param in parameters if param.get("id") == "lfo_shape"]
        if len(shapes) != 1:
            problems.append("generated lfo_shape missing or duplicated")
        else:
            shape = shapes[0]
            if (shape.get("default"), shape.get("minimum"),
                    shape.get("maximum"), shape.get("step")) != (0, 0, 3, 1):
                problems.append("lfo_shape range/default changed")
            choices = [(item.get("value"), item.get("label"))
                       for item in shape.get("choices", [])]
            if choices != list(enumerate(cls.SHAPES)):
                problems.append("lfo_shape choices changed")

        if panel.get("code") != "CHAOS":
            problems.append("CHAOS panel code changed")
        panel_ids = [name for section in panel.get("sections", [])
                     for name in section.get("params", [])]
        if len(panel_ids) != len(set(panel_ids)) or set(panel_ids) != set(expected):
            problems.append("CHAOS panel parameters missing, duplicate, or unknown")
        return problems

    @staticmethod
    def metadata():
        root = ROOT
        return (
            json.loads((root / "native/modules2/chaos_lite/module.json").read_text(encoding='utf-8')),
            json.loads((root / "apps/studio/public/wirescape_modules.json").read_text(encoding='utf-8')),
            json.loads((root / "apps/studio/public/panels/essentials.chaos_lite.json").read_text(encoding='utf-8')),
        )

    def test_current_chaos_descriptor_and_panel_match(self):
        self.assertEqual(self.problems(*self.metadata()), [])

    def test_mutated_native_slot_and_panel_param_are_detected(self):
        native, catalogue, panel = self.metadata()
        bad_native = json.loads(json.dumps(native))
        bad_native["parameters"]["lfo_shape"] = 25
        self.assertIn("native parameter slot changed",
                      self.problems(bad_native, catalogue, panel))
        bad_panel = json.loads(json.dumps(panel))
        bad_panel["sections"][0]["params"][0] = "unknown_parameter"
        self.assertIn("CHAOS panel parameters missing, duplicate, or unknown",
                      self.problems(native, catalogue, bad_panel))
        bad_catalogue = json.loads(json.dumps(catalogue))
        module = next(m for m in bad_catalogue["modules"]
                      if m["id"] == "essentials.chaos_lite")
        next(p for p in module["parameters"] if p["id"] == "lfo_shape")["choices"].pop()
        self.assertIn("lfo_shape choices changed",
                      self.problems(native, bad_catalogue, panel))
