# SPDX-License-Identifier: GPL-3.0-only
"""Read-only Pi prerequisite checks and sanitized source-build stage regressions."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("pi_preflight", ROOT / "tools/pi/preflight.py")
preflight = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(preflight)
SHA = "1234567890abcdef1234567890abcdef12345678"


class PiPreflightTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="pi preflight path with spaces ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for app in ("studio", "desktop"):
            folder = self.root / "apps" / app
            folder.mkdir(parents=True)
            (folder / "package-lock.json").write_text("{}")
        self.tool_info = {name: {"available": True, "version": "v24.10.0" if name == "node" else "1.0"}
                          for name in ("git", "c++", "cmake", "ninja", "node", "npm", "python")}
        self.patches = [
            patch.object(preflight, "versions", return_value=self.tool_info),
            patch.object(preflight, "discover_toolchain", return_value={"generator": "Ninja"}),
            patch.object(preflight.platform, "system", return_value="Linux"),
            patch.object(preflight.platform, "machine", return_value="aarch64"),
            patch.object(preflight.ctypes.util, "find_library", return_value="libpresent.so.1"),
            patch.object(preflight.shutil, "disk_usage", return_value=shutil._ntuple_diskusage(10**10, 10**9, 9*10**9)),
            patch.object(preflight.os, "access", return_value=True),
            patch.object(preflight, "git", side_effect=lambda root, *args: (0, SHA) if args[0] == "rev-parse" else (0, "")),
            patch.dict(os.environ, {key: "" for key in ("CMAKE_PREFIX_PATH", "Qt6_DIR", "QT_PLUGIN_PATH", "QML2_IMPORT_PATH", "WIRESCAPE_QT_DIR")}),
        ]
        for p in self.patches:
            p.start()
            self.addCleanup(p.stop)

    def failures(self):
        return {name for name, ok, _ in preflight.inspect(self.root)[0] if not ok}

    def test_check_only_is_read_only_even_with_spaces(self):
        before = {str(p.relative_to(self.root)): p.read_bytes() for p in self.root.rglob("*") if p.is_file()}
        self.assertEqual(preflight.preflight(self.root), 0)
        after = {str(p.relative_to(self.root)): p.read_bytes() for p in self.root.rglob("*") if p.is_file()}
        self.assertEqual(before, after)
        self.assertFalse((self.root / "build_reports").exists())

    def test_wrong_architecture_and_missing_compiler(self):
        self.tool_info["c++"]["available"] = False
        with patch.object(preflight.platform, "machine", return_value="x86_64"):
            self.assertTrue({"Linux ARM64", "c++"} <= self.failures())

    def test_unsupported_node_and_missing_lockfile(self):
        self.tool_info["node"]["version"] = "v23.3.0"
        (self.root / "apps/desktop/package-lock.json").unlink()
        self.assertTrue({"Node maintained LTS", "apps/desktop/package-lock.json"} <= self.failures())

    def test_dirty_tree_and_missing_portaudio(self):
        with patch.object(preflight, "git", return_value=(0, " M owner.txt")), \
             patch.object(preflight.ctypes.util, "find_library", return_value=None):
            self.assertTrue({"clean tree", "lib:portaudio"} <= self.failures())

    def test_stale_arch_and_checkout_cache_refused_without_removal(self):
        (self.root / "build").mkdir()
        cache = self.root / "build/CMakeCache.txt"
        cache.write_text("CMAKE_HOME_DIRECTORY:INTERNAL=/elsewhere/native\nCMAKE_SYSTEM_PROCESSOR:STRING=x86_64\nQt6_DIR:PATH=/old/qt\n")
        self.assertIn("cache:build/CMakeCache.txt", self.failures())
        self.assertTrue(cache.is_file())

    def own_cache(self):
        (self.root / "build/native").mkdir(parents=True, exist_ok=True)
        cache = self.root / "build/native/CMakeCache.txt"
        cache.write_text(
            "CMAKE_HOME_DIRECTORY:INTERNAL=" + str((self.root / "native").resolve()) + "\n"
            "CMAKE_SYSTEM_PROCESSOR:STRING=aarch64\n"
            "CMAKE_GENERATOR:INTERNAL=Ninja\n"
            "WIRESCAPE_BUILD_UI:BOOL=OFF\n"
            "Qt6_DIR:PATH=Qt6_DIR-NOTFOUND\n"
        )
        return cache

    def test_own_headless_ninja_cache_passes_and_notfound_is_not_qt(self):
        cache = self.own_cache()
        before = cache.read_bytes()
        self.assertNotIn("cache:build/native/CMakeCache.txt", self.failures())
        self.assertEqual(cache.read_bytes(), before)

    def test_wrong_generator_or_ui_enabled_cache_is_rejected(self):
        cache = self.own_cache()
        original = cache.read_text()
        for changed in (
            original.replace("CMAKE_GENERATOR:INTERNAL=Ninja", "CMAKE_GENERATOR:INTERNAL=Unix Makefiles"),
            original.replace("WIRESCAPE_BUILD_UI:BOOL=OFF", "WIRESCAPE_BUILD_UI:BOOL=ON"),
            original.replace("Qt6_DIR:PATH=Qt6_DIR-NOTFOUND", "Qt6_DIR:PATH=/old/qt"),
            original.replace("CMAKE_SYSTEM_PROCESSOR:STRING=aarch64", "CMAKE_SYSTEM_PROCESSOR:STRING=x86_64"),
            original.replace(str((self.root / "native").resolve()), "/other/checkout/native"),
        ):
            with self.subTest(cache=changed.splitlines()[-2:]):
                cache.write_text(changed)
                self.assertIn("cache:build/native/CMakeCache.txt", self.failures())
                self.assertEqual(cache.read_text(), changed)

    def test_first_check_only_process_creates_no_bytecode_or_reports(self):
        # Snapshot before importing the entrypoint in a fresh interpreter.
        # An in-process snapshot cannot detect helpers.__pycache__ from import.
        source = ROOT / "tools"
        pi = self.root / "tools/pi"
        foundation = self.root / "tools/foundation"
        pi.mkdir(parents=True)
        foundation.mkdir(parents=True)
        shutil.copy2(source / "pi/preflight.py", pi / "preflight.py")
        shutil.copy2(source / "foundation/helpers.py", foundation / "helpers.py")
        before = {str(p.relative_to(self.root)): p.read_bytes()
                  for p in self.root.rglob("*") if p.is_file()}
        env = os.environ.copy()
        env.pop("PYTHONDONTWRITEBYTECODE", None)
        env.pop("PYTHONPYCACHEPREFIX", None)
        result = subprocess.run(
            [sys.executable, str(pi / "preflight.py"), "--check-only", "--root", str(self.root)],
            cwd=self.root, env=env, text=True, capture_output=True, timeout=30, check=False)
        self.assertNotEqual(result.returncode, 0)  # non-Pi fixture is blocked, not passed
        self.assertIn("Preflight:", result.stdout)
        after = {str(p.relative_to(self.root)): p.read_bytes()
                 for p in self.root.rglob("*") if p.is_file()}
        self.assertEqual(after, before)
        self.assertFalse((foundation / "__pycache__").exists())
        self.assertFalse((self.root / "build_reports").exists())

    def test_inherited_qt_state_is_rejected_without_modifying_env(self):
        with patch.dict(os.environ, {"QT_PLUGIN_PATH": "/old/qt"}):
            self.assertIn("no inherited Qt overrides", self.failures())
            self.assertEqual(os.environ["QT_PLUGIN_PATH"], "/old/qt")

    @unittest.skipIf(os.name == "nt", "Pi source-build stage runner uses POSIX process groups")
    def test_native_and_npm_failures_keep_codes_and_redact_logs(self):
        report = self.root / "build_reports" / "pi source" / SHA
        script = "import sys; print('Bearer secret-value'); print('checkout works'); sys.exit(7)"
        for stage in ("native", "studio-build"):
            with self.subTest(stage=stage):
                code = preflight.stage_run(self.root, report, stage, [sys.executable, "-c", script], 10)
                self.assertEqual(code, 7)
                log = (report / f"{stage}.log").read_text()
                self.assertIn("Bearer <redacted>", log)
                self.assertNotIn("secret-value", log)
                self.assertEqual(json.loads((report / "summary.json").read_text())["stages"][stage]["exit_code"], 7)
        self.assertEqual(json.loads((report / "summary.json").read_text())["stages"]["desktop-ci"]["status"], "not-run")

    @unittest.skipIf(os.name == "nt", "Pi source-build stage runner uses POSIX process groups")
    def test_stage_timeout_ends_owned_child_and_reports_failure(self):
        report = self.root / "build_reports" / "smoke"
        code = preflight.stage_run(self.root, report, "native", [sys.executable, "-c", "import time; time.sleep(4)"], 1)
        self.assertEqual(code, 124)
        self.assertIn("timed out", (report / "native.log").read_text())

    @unittest.skipIf(os.name == "nt", "Pi source-build stage runner uses POSIX process groups")
    def test_success_provenance_and_missing_artifact_are_not_confused(self):
        report = self.root / "build_reports" / SHA
        cmd = [sys.executable, "-c", "print('synthetic build complete')"]
        self.assertEqual(preflight.stage_run(self.root, report, "native", cmd, 10), 1)
        engine = self.root / "build/native/wirescape-engine"
        engine.parent.mkdir(parents=True)
        engine.write_text("mock executable")
        self.assertEqual(preflight.stage_run(self.root, report, "native", cmd, 10), 0)
        summary = json.loads((report / "summary.json").read_text())
        self.assertEqual(summary["source_sha"], SHA)
        self.assertEqual(summary["architecture"], "aarch64")
        self.assertEqual(summary["stages"]["native"]["exit_code"], 0)
        self.assertEqual(summary["stages"]["studio-build"]["status"], "not-run")

    def test_no_device_flags_and_no_external_reports(self):
        self.assertEqual(preflight.stage_run(self.root, self.root / "elsewhere", "native", ["true"], 1), 2)
        self.assertEqual(preflight.stage_run(self.root, self.root / "build_reports", "native", ["engine", "--confirm-audio"], 1), 2)


if __name__ == "__main__":
    unittest.main()
