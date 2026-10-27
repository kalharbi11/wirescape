"""Keep synthetic migration inputs usable without importing the legacy runtime."""
import json
import unittest
from pathlib import Path


class LegacyFixtureInventoryTests(unittest.TestCase):
    def test_evidenced_shapes_and_preservation_payloads(self):
        folder = Path(__file__).resolve().parents[3] / "native/tests/fixtures/legacy_documents"
        documents = {p.name: json.loads(p.read_text(encoding="utf-8")) for p in folder.glob("*.json")}
        expected = {"patch_v1.json": ("wirescape.patch", 1), "patch_v2.json": ("wirescape.patch", 2),
                    "patch_v3_unknown.json": ("wirescape.patch", 3), "visual_study_v4.json": ("wirescape.visual.study", 4),
                    "future_patch_v99.json": ("wirescape.patch", 99), "malformed_patch.json": ("wirescape.patch", 3)}
        self.assertEqual(set(documents), set(expected))
        for name, identity in expected.items():
            document = documents[name]
            self.assertEqual((document["format"], document["version"]), identity)
            if name != "malformed_patch.json":
                self.assertIsInstance(document["modules"], list)
                self.assertIsInstance(document["connections"], list)
                ids = [module["id"] for module in document["modules"]]
                self.assertEqual(len(ids), len(set(ids)))
        self.assertNotIsInstance(documents["malformed_patch.json"]["modules"], list)
        unknown = documents["patch_v3_unknown.json"]
        self.assertTrue(unknown["vendor_document_field"])
        self.assertTrue(unknown["modules"][1]["opaque_state"])
        self.assertTrue(unknown["connections"][0]["vendor_cable_field"])


if __name__ == "__main__":
    unittest.main()
