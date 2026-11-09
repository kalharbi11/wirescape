# SPDX-License-Identifier: GPL-3.0-only
"""Patch fast-lane checks; never open devices."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("patch_library", ROOT / "tools/patch_library.py")
library = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(library)


class PatchLibraryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalogue = json.loads((ROOT / "apps/studio/public/wirescape_modules.json").read_text())
        cls.example = next(p['patch'] for p in json.loads((ROOT / 'native/document/ui_starters.json').read_text()) if p['id'] == 'keyboard-voice')

    def test_all_shared_patches_validate(self):
        for path in (ROOT / "apps/studio/patches").glob("*.json"):
            with self.subTest(path=path):
                library.read_patch(path, self.catalogue, True)

    def test_export_and_unknown_data_survive_install(self):
        data = copy.deepcopy(self.example)
        data["future"] = {"opaque": [None, "λ", False]}
        raw = json.dumps(data, ensure_ascii=False).encode()
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "source.json"
            source.write_bytes(raw)
            validated, _ = library.read_patch(source, self.catalogue, True)
            dest = Path(folder) / "installed"
            library.install(source, dest, validated)
            self.assertEqual((dest / source.name).read_bytes(), raw)

    def test_shared_name_length_is_rejected(self):
        patch = copy.deepcopy(self.example)
        patch["name"] = "x" * 241
        with self.assertRaises(ValueError):
            library.validate(patch, self.catalogue, True)

    def test_graph_errors_are_rejected(self):
        broken = []
        p = copy.deepcopy(self.example)
        p["modules"].append(copy.deepcopy(p["modules"][0]))
        broken.append(p)
        p = copy.deepcopy(self.example)
        p["connections"][0]["to"]["module"] = 999
        broken.append(p)
        p = copy.deepcopy(self.example)
        p["connections"][0]["kind"] = "event"
        broken.append(p)
        p = copy.deepcopy(self.example)
        p["modules"][1]["parameters"]["frequency"] = float("nan")
        broken.append(p)
        p = copy.deepcopy(self.example)
        p["modules"][1]["parameters"]["frequency"] = 1e20
        broken.append(p)
        p = copy.deepcopy(self.example)
        p["outputs"][0]["bus_channel"] = 16
        broken.append(p)
        for patch in broken:
            with self.subTest(patch=patch), self.assertRaises(ValueError):
                library.validate(patch, self.catalogue)

    def test_shared_device_bindings_rejected_but_personal_data_preserved(self):
        patch = copy.deepcopy(self.example)
        patch["devices"] = [{"kind": "midi", "name": "private endpoint"}]
        library.validate(patch, self.catalogue)
        with self.assertRaises(ValueError):
            library.validate(patch, self.catalogue, True)

    def test_no_clobber_and_explicit_replacement_keeps_backup(self):
        with tempfile.TemporaryDirectory() as folder:
            dest = Path(folder)
            source = Path("voice.json")
            library.install(source, dest, b"original")
            with self.assertRaises(ValueError):
                library.install(source, dest, b"new")
            self.assertEqual((dest / source.name).read_bytes(), b"original")
            library.install(source, dest, b"new", True)
            self.assertEqual((dest / source.name).read_bytes(), b"new")
            self.assertEqual([p.read_bytes() for p in dest.glob("*.bak")], [b"original"])


if __name__ == "__main__":
    unittest.main()

