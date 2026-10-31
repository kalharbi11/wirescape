# SPDX-License-Identifier: GPL-3.0-only
"""Headless launch provenance regression tests."""

import argparse
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import dev  # noqa: E402


class LaunchFreshnessTests(unittest.TestCase):
    def test_launch_requires_matching_gui_evidence(self):
        cases = {
            "matching": (None, 0),
            "source_changed": ("source", 2),
            "binary_changed": ("binary", 2),
            "missing_metadata": ("missing", 2),
            "corrupt_metadata": ("corrupt", 2),
            "config_mismatch": ("config", 2),
            "headless_build": ("headless", 2),
            "ui_disabled": ("ui_off", 2),
        }
        for name, (change, expected) in cases.items():
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                build = root / "build"
                build.mkdir()
                exe = build / "wirescape_studio.exe"
                exe.write_bytes(b"verified")
                metadata = {
                    "configuration": "Release", "ui_verified": True, "source_stable": True,
                    "source": {"sha256": "original"},
                    "binaries": {"wirescape_studio.exe": dev.sha256_file(exe)},
                }
                cache = "WIRESCAPE_BUILD_UI:BOOL=ON\n"
                source = "original"
                if change == "source":
                    source = "changed"
                elif change == "binary":
                    exe.write_bytes(b"altered")
                elif change == "config":
                    metadata["configuration"] = "Debug"
                elif change == "headless":
                    metadata["ui_verified"] = False
                elif change == "ui_off":
                    cache = "WIRESCAPE_BUILD_UI:BOOL=OFF\n"
                (build / "CMakeCache.txt").write_text(cache)
                if change != "missing":
                    (build / "build-metadata.json").write_text(
                        "not json" if change == "corrupt" else json.dumps(metadata))
                captured = []
                def save(record):
                    captured.append(record)
                    return root / "report.json"
                with patch.object(dev, "ROOT", root), \
                     patch.object(dev, "BUILD_DIR", build), \
                     patch.object(dev, "_studio_executable", return_value=exe), \
                     patch.object(dev, "_base_record", return_value={"action": "launch"}), \
                     patch.object(dev, "source_provenance", return_value={"sha256": source}), \
                     patch.object(dev, "_save_record", side_effect=save), \
                     patch.object(dev, "_print_record"), \
                     patch.object(dev, "_qt_runtime_env", return_value={}), \
                     patch.object(dev, "wait_for_readiness", return_value={"ready": True}) as ready:
                    result = dev.command_launch(argparse.Namespace(config="Release", json=False))
                self.assertEqual(result, expected)
                if expected:
                    ready.assert_not_called()
                    self.assertIn("python tools/dev.py build --config Release", captured[0]["summary"])
                else:
                    ready.assert_called_once()
                    self.assertEqual(ready.call_args.args[0], [str(exe), "--interactive-ready-report"])
                    self.assertIn("audio has not started", captured[0]["summary"])

    def test_failed_build_invalidates_previous_gui_certificate(self):
        for results in ([{"exit_code": 7}], [{"exit_code": 0}, {"exit_code": 7}]):
            with self.subTest(stage=len(results)), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                (root / "native").mkdir()
                (root / "native/CMakeLists.txt").touch()
                build = root / "build"
                build.mkdir()
                metadata = build / "build-metadata.json"
                metadata.write_text(json.dumps({"ui_verified": True}))
                with patch.object(dev, "ROOT", root), \
                     patch.object(dev, "BUILD_DIR", build), \
                     patch.object(dev, "_base_record", return_value={"source": {}}), \
                     patch.object(dev, "_cmake_and_generator", return_value=("cmake", "Ninja", "x64")), \
                     patch.object(dev, "_qt_prefix", return_value=None), \
                     patch.object(dev, "run_logged", side_effect=results), \
                     patch.object(dev, "_save_record"), patch.object(dev, "_print_record"):
                    args = argparse.Namespace(config="Release", qt_prefix=None, headless=False, json=False)
                    self.assertEqual(dev.command_build(args), 7)
                self.assertIs(json.loads(metadata.read_text())["ui_verified"], False)

    def test_metadata_refresh_cannot_certify_leftover_gui(self):
        for ui_built, cache_value, expected in ((False, "ON", False),
                                                (True, "OFF", False),
                                                (True, "ON", True)):
            with self.subTest(ui_built=ui_built, cache=cache_value), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                build = root / "build"
                build.mkdir()
                (build / "CMakeCache.txt").write_text(f"WIRESCAPE_BUILD_UI:BOOL={cache_value}\n")
                (build / "wirescape_studio.exe").write_bytes(b"leftover")
                with patch.object(dev, "ROOT", root), \
                     patch.object(dev, "BUILD_DIR", build), \
                     patch.object(dev, "source_provenance", return_value={"sha256": "same"}):
                    metadata, stable = dev._refresh_build_metadata(
                        "Release", {"sha256": "same"}, ui_built=ui_built)
                self.assertTrue(stable)
                self.assertIs(metadata["ui_verified"], expected)


if __name__ == "__main__":
    unittest.main()
