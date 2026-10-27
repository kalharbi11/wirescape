import hashlib
import json
import tempfile
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from foundation.package_inventory import inspect_package, main


class PackageInventoryTests(unittest.TestCase):
    def test_reports_hash_drift_missing_and_unexpected(self):
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as temp:
            root = Path(temp)
            (root / "kept.txt").write_text("changed", encoding="utf-8")
            expected_hash = hashlib.sha256(b"original").hexdigest()
            (root / "package-manifest.json").write_text(json.dumps({"files": {"kept.txt": expected_hash, "gone.txt": "0" * 64}}), encoding="utf-8")
            (root / "extra.txt").write_text("extra", encoding="utf-8")

            report = inspect_package(root)
            self.assertEqual(report["integrity"]["hash_mismatches"], ["kept.txt"])
            self.assertEqual(report["integrity"]["missing"], ["gone.txt"])
            self.assertEqual(report["integrity"]["unexpected"], ["extra.txt"])
            self.assertFalse(report["integrity"]["ok"])

    def test_cli_writes_json_and_returns_nonzero_on_drift(self):
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as temp:
            root = Path(temp)
            (root / "file.txt").write_text("x", encoding="utf-8")
            (root / "package-manifest.json").write_text(json.dumps({"files": {}}), encoding="utf-8")
            output = root / "report.json"
            self.assertEqual(main(["--package", str(root), "--output", str(output)]), 1)
            self.assertFalse(json.loads(output.read_text(encoding="utf-8"))["integrity"]["ok"])

    def test_valid_fixture_ignores_manifest_file_itself(self):
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as temp:
            root = Path(temp)
            (root / "LICENSE").write_text("notice", encoding="utf-8")
            digest = hashlib.sha256(b"notice").hexdigest()
            (root / "package-manifest.json").write_text(json.dumps({"files": {"LICENSE": digest}}), encoding="utf-8")
            report = inspect_package(root)
            self.assertTrue(report["integrity"]["ok"])
            self.assertEqual(report["runtime_inventory"]["qt"]["version"], "unknown")


if __name__ == "__main__":
    unittest.main()
