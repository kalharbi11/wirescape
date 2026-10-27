import json
import tempfile
import unittest
from pathlib import Path

from verify_submission import verify


class VerifySubmissionTests(unittest.TestCase):
    def make_submission(self, root, manifest=None):
        for filename in ("module.cpp", "CMakeLists.txt", "tests.cpp"):
            (root / filename).write_text("// placeholder\n", encoding="utf-8")
        (root / "module.json").write_text(json.dumps(manifest or {
            "schema_version": 1, "id": "team.effect", "name": "Effect", "abi_version": 1,
            "library": "ws_effect", "parameters": {"amount": 0},
            "inputs": {"audio": "audio"}, "outputs": {"audio": "audio"},
        }), encoding="utf-8")

    def test_accepts_structurally_valid_folder(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1] / "smoke_tests") as temp:
            root = Path(temp) / "candidate"
            root.mkdir()
            self.make_submission(root)
            self.assertEqual([], verify(root))

    def test_rejects_bad_manifest_and_missing_files(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1] / "smoke_tests") as temp:
            root = Path(temp)
            self.make_submission(root, {"schema_version": 9})
            (root / "tests.cpp").unlink()
            problems = verify(root)
            self.assertTrue(any("schema_version" in p for p in problems))
            self.assertTrue(any("required regular file is missing: tests.cpp" in p for p in problems))

    def test_rejects_duplicate_parameter_ids(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1] / "smoke_tests") as temp:
            root = Path(temp)
            self.make_submission(root)
            p = root / "module.json"
            data = json.loads(p.read_text(encoding="utf-8"))
            data["parameters"] = {"a": 0, "b": 0}
            p.write_text(json.dumps(data), encoding="utf-8")
            self.assertTrue(any("unique" in x for x in verify(root)))

    def test_rejects_prebuilt_binary_and_generated_tree(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1] / "smoke_tests") as temp:
            root = Path(temp)
            self.make_submission(root)
            (root / "payload.dll").write_bytes(b"not executed")
            (root / "build").mkdir()
            problems = verify(root)
            self.assertTrue(any("prebuilt or bundled" in p for p in problems))
            self.assertTrue(any("generated or repository directory" in p for p in problems))


if __name__ == "__main__":
    unittest.main()
