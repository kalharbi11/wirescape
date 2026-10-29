# SPDX-License-Identifier: GPL-3.0-only
import copy
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("golden", Path(__file__).resolve().parents[2] / "golden.py")
golden = importlib.util.module_from_spec(spec)
spec.loader.exec_module(golden)


class GoldenTests(unittest.TestCase):
    def setUp(self):
        self.report = {"sample_rate": 48000, "frames": 48000, "block": 128, "seed": 42,
                       "cases": [{"name": name, "first": "a" * 64, "second": "a" * 64}
                                 for name in ("patch/demo", "module/default")]}

    def test_valid_same_build_hashes_ignore_previous_platform_reports(self):
        self.assertEqual(golden.validate(self.report), self.report)
        changed = copy.deepcopy(self.report)
        for case in changed["cases"]:
            case["first"] = case["second"] = "b" * 64
        self.assertEqual(golden.validate(changed), changed)

    def test_mismatch_missing_coverage_duplicate_bad_hash_and_settings_fail(self):
        for mode in ("mismatch", "empty", "duplicate", "bad_hash", "settings"):
            with self.subTest(mode=mode):
                report = copy.deepcopy(self.report)
                if mode == "mismatch":
                    report["cases"][0]["second"] = "b" * 64
                elif mode == "empty":
                    report["cases"] = []
                elif mode == "duplicate":
                    report["cases"].append(report["cases"][0])
                elif mode == "bad_hash":
                    report["cases"][0]["first"] = report["cases"][0]["second"] = "invalid"
                else:
                    report["seed"] = 1
                with self.assertRaises(ValueError):
                    golden.validate(report)
