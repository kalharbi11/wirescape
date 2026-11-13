# SPDX-License-Identifier: GPL-3.0-only
import copy
import importlib.util
import hashlib
import struct
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

    def test_canonical_bytes_hash_identically_and_changed_bytes_fail(self):
        # Frozen IEEE values include negative zero, which must retain its sign bit.
        data = struct.pack("<4f", 0.0, -0.0, -0.5, 1.0)
        report = copy.deepcopy(self.report)
        for case in report["cases"]:
            case.pop("first")
            case.pop("second")
            case["first_bytes"] = case["second_bytes"] = data.hex()
        hashed = golden.hash_streams(copy.deepcopy(report))
        golden.validate(hashed)
        self.assertEqual(hashed["cases"][0]["first"], hashlib.sha256(data).hexdigest())
        report["cases"][0]["second_bytes"] = (data + b"\0").hex()
        with self.assertRaises(ValueError):
            golden.validate(golden.hash_streams(report))

    def test_malformed_native_byte_stream_refuses(self):
        for bad in ("0", "not-hex"):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                golden.hash_streams({"first_bytes": bad})
