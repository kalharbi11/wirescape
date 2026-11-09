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
        temp = tempfile.TemporaryDirectory(prefix="pi preflight path with spaces ")
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        for app in ("studio", "desktop"):
            folder = self.root / "apps" / app
            folder.mkdir(parents=True)
            (folder / "package-lock.json").write_text("{}")
        self.tool_info = {name: {"available": True, "version": "v24.10.0" if name == "node" else "1.0"}
                          for name in ("git", "c++", "cmake", "ninja", "node", "npm", "python")}
        mocks = [
            patch.object(preflight, "versions", return_value=self.tool_info),
            patch.object(preflight, "discover_toolchain", return_value={"generator": "Ninja"}),
            patch.object(preflight.platform, "system", return_value="Linux"),
            patch.object(preflight.platform, "machine", return_value="aarch64"),
            patch.object(preflight.ctypes.util, "find_library", return_value="libpresent.so.1"),
            patch.object(preflight.shutil, "disk_usage", return_value=shutil._ntuple_diskusage(10**10, 10**9, 9*10**9)),
            patch.object(preflight.os, "access", return_value=True),
            patch.object(preflight, "git", side_effect=lambda root, *args: (0, SHA) if args[0] == "rev-parse" else (0, "")),
            patch.dict(os.environ, {k: "" for k in ("CMAKE_PREFIX_PATH", "Qt6_DIR", "QT_PLUGIN_PATH", "QML2_IMPORT_PATH", "WIRESCAPE_QT_DIR")}),
        ]
        for mock in mocks:
            mock.start()
            self.addCleanup(mock.stop)

    def failures(self):
        return {name for name, ok, _ in preflight.inspect(self.root)[0] if not ok}

    def test_check_only_is_read_only_even_with_spaces(self):
        def snapshot():
            return {str(p.relative_to(self.root)): p.read_bytes()
                    for p in self.root.rglob("*") if p.is_file()}
        before = snapshot()
        self.assertEqual(preflight.preflight(self.root), 0)
        self.assertEqual(before, snapshot())
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
        cache.write_text("CMAKE_HOME_DIRECTORY:INTERNAL=" + str((self.root / "native").resolve()) +
                         "\nCMAKE_SYSTEM_PROCESSOR:STRING=aarch64\nCMAKE_GENERATOR:INTERNAL=Ninja"
                         "\nWIRESCAPE_BUILD_UI:BOOL=OFF\nQt6_DIR:PATH=Qt6_DIR-NOTFOUND\n")
        return cache

    def test_own_headless_ninja_cache_passes_and_notfound_is_not_qt(self):
        cache = self.own_cache()
        before = cache.read_bytes()
        self.assertNotIn("cache:build/native/CMakeCache.txt", self.failures())
        self.assertEqual(cache.read_bytes(), before)

    def test_own_headless_system_qtcore_cache_and_symlink_escape(self):
        cache = self.own_cache()
        qt = (self.root / 'usr/lib/aarch64-linux-gnu/cmake/Qt6').resolve()
        qt.mkdir(parents=True)
        config = qt / 'Qt6Config.cmake'
        config.write_text('# discovered QtCore package')
        cache.write_text(cache.read_text().replace('Qt6_DIR-NOTFOUND', str(qt)))
        with patch.object(preflight, 'SYSTEM_QT_DIRS', (qt,)):
            self.assertNotIn('cache:build/native/CMakeCache.txt', self.failures())
            if os.name != 'nt':
                foreign = self.root / 'foreign-Qt6Config.cmake'
                foreign.write_text('# foreign SDK')
                config.unlink()
                config.symlink_to(foreign)
                self.assertIn('cache:build/native/CMakeCache.txt', self.failures())

    def test_wrong_generator_or_ui_enabled_cache_is_rejected(self):
        cache = self.own_cache()
        original = cache.read_text()
        cases = (
            ("Ninja", "Unix Makefiles"), ("WIRESCAPE_BUILD_UI:BOOL=OFF", "WIRESCAPE_BUILD_UI:BOOL=ON"),
            ("Qt6_DIR-NOTFOUND", "/old/qt"), ("aarch64", "x86_64"),
            ("Qt6_DIR:PATH=Qt6_DIR-NOTFOUND", "CMAKE_PREFIX_PATH:STRING=token=private-word"),
            ("WIRESCAPE_BUILD_UI:BOOL=OFF", ""),
            (str((self.root / "native").resolve()), "/other/checkout/native"),
        )
        for old, new in cases:
            with self.subTest(old=old, new=new):
                changed = original.replace(old, new)
                cache.write_text(changed)
                self.assertIn("cache:build/native/CMakeCache.txt", self.failures())
                self.assertEqual(cache.read_text(), changed)
                reason = next(row[2] for row in preflight.inspect(self.root)[0]
                              if row[0] == 'cache:build/native/CMakeCache.txt')
                self.assertIn('Cache mismatch (', reason)
                self.assertNotIn('private-word', reason)

    def test_first_check_only_process_creates_no_bytecode_or_reports(self):
        # Snapshot before any fresh-process helper import, including blocked exits.
        source, tools = ROOT / "tools", self.root / "tools"
        for folder, filename in (("pi", "preflight.py"), ("foundation", "helpers.py")):
            dest = tools / folder
            dest.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source / folder / filename, dest / filename)
        def snapshot():
            return {str(p.relative_to(self.root)): p.read_bytes()
                    for p in self.root.rglob("*") if p.is_file()}
        before = snapshot()
        env = {k: v for k, v in os.environ.items() if k not in
               ("PYTHONDONTWRITEBYTECODE", "PYTHONPYCACHEPREFIX")}
        result = subprocess.run(
            [sys.executable, str(tools / "pi/preflight.py"), "--check-only", "--root", str(self.root)],
            cwd=self.root, env=env, text=True, capture_output=True, timeout=30, check=False)
        self.assertNotEqual(result.returncode, 0)  # x86 fixture is blocked
        self.assertIn("Preflight:", result.stdout)
        self.assertEqual(snapshot(), before)
        self.assertFalse((tools / "foundation/__pycache__").exists())
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
