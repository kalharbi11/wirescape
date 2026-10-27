from __future__ import annotations
import json
import sys
import tempfile
import unittest
import argparse
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import dev
TEMP_ROOT = Path(__file__).resolve().parents[3] / "build_reports/foundation"
TEMP_ROOT.mkdir(parents=True, exist_ok=True)
from foundation.helpers import run_logged, source_provenance


class ReportingTests(unittest.TestCase):
    def test_doctor_missing_native_project_is_failure(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder)
            toolchain = {"cmake": "cmake", "generator": "Ninja", "architecture": "x64",
                         "ninja": "ninja", "make": None, "visual_studio": None}
            with patch.object(dev, "ROOT", root), \
                 patch.object(dev, "_base_record", return_value={"action": "doctor"}), \
                 patch.object(dev, "discover_toolchain", return_value=toolchain), \
                 patch.object(dev, "_save_record", return_value=root / "doctor.json"):
                self.assertEqual(dev.command_doctor(argparse.Namespace(json=True)), 1)

    def test_native_suite_failure_is_nonzero_and_prints_suite(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder)
            (root / "CMakeCache.txt").touch()
            log = root / "ctest.log"
            log.write_text("1/1 Test #1: abi2_engine_tests ...***Failed 0.01 sec\n")
            output = __import__("io").StringIO()
            with patch.object(dev, "BUILD_DIR", root), \
                 patch.object(dev, "_base_record", return_value={"action": "test", "source": {}}), \
                 patch.object(dev, "_refresh_build_metadata", return_value=({}, True)), \
                 patch.object(dev, "discover_toolchain", return_value={"cmake": "cmake", "ctest": "ctest"}), \
                 patch.object(dev, "run_logged", side_effect=[{"exit_code": 0}, {"exit_code": 8, "log": str(log)}]), \
                 patch.object(dev, "_save_record", return_value=root / "test.json"), \
                 __import__("contextlib").redirect_stdout(output):
                self.assertEqual(dev.command_test(argparse.Namespace(config="Release", json=False)), 8)
            self.assertIn("abi2_engine_tests", output.getvalue())
            self.assertIn("***Failed", output.getvalue())

    def test_benchmark_failure_preserves_exit_status(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder)
            with patch.object(dev, "_base_record", return_value={"action": "benchmark"}), \
                 patch.object(dev, "_native_cli", return_value=root / "cli"), \
                 patch.object(dev, "_unique_report_path", return_value=root / "missing.json"), \
                 patch.object(dev, "run_logged", return_value={"exit_code": 7}), \
                 patch.object(dev, "_save_record", return_value=root / "report.json"):
                self.assertEqual(dev.command_benchmark(argparse.Namespace(config="Release", blocks=2, json=True)), 7)

    def test_functional_only_defers_benchmark_and_default_check_keeps_it(self):
        for functional_only in (True, False):
            with self.subTest(functional_only=functional_only), tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
                root = Path(folder)
                saved_path = root / "check.json"
                def save(record):
                    saved_path.write_text(json.dumps(record), encoding="utf-8")
                    return saved_path
                benchmark = unittest.mock.Mock(return_value=0)
                with patch.object(dev, "REPORT_DIR", root), \
                     patch.object(dev, "_base_record", return_value={"action": "check"}), \
                     patch.object(dev, "_save_record", side_effect=save), \
                     patch.object(dev, "_unique_report_path", side_effect=lambda stem, suffix: root / f"{stem}{suffix}"), \
                     patch.object(dev.unittest.defaultTestLoader, "discover", return_value=unittest.TestSuite()), \
                     patch.object(dev, "command_build", return_value=0), \
                     patch.object(dev, "command_test", return_value=0), \
                     patch.object(dev, "command_ui_test", return_value=0), \
                     patch.object(dev, "command_startup_test", return_value=0), \
                     patch.object(dev, "command_benchmark", benchmark), \
                     patch.object(dev, "command_render", return_value=0), \
                     patch.object(dev, "command_package", return_value=0):
                    args = argparse.Namespace(config="Release", package=True, json=True)
                    if functional_only:
                        args.functional_only = True
                    self.assertEqual(dev.command_check(args), 0)
                result = json.loads(saved_path.read_text(encoding="utf-8"))
                bench_stage = next(stage for stage in result["stages"] if stage["name"] == "benchmark")
                if functional_only:
                    benchmark.assert_not_called()
                    self.assertEqual(bench_stage["status"], "deferred")
                    self.assertIn("deferred", bench_stage["reason"])
                    self.assertIn("benchmark deferred", result["summary"])
                    self.assertNotIn("Checkpoint passed", result["summary"])
                    markdown = Path(result["markdown_summary"]).read_text(encoding="utf-8")
                    self.assertIn("| benchmark | deferred |", markdown)
                    self.assertIn("no performance result recorded", result["summary"])
                else:
                    benchmark.assert_called_once()
                    self.assertEqual(bench_stage["exit_code"], 0)
                    self.assertEqual(result["summary"], "Checkpoint passed")

    def test_portable_smoke_and_startup_reports_require_audio_off_truth(self):
        self.assertTrue(dev._valid_smoke_report({"status": "passed", "audio_started": False}))
        self.assertFalse(dev._valid_smoke_report({"status": "passed"}))
        self.assertFalse(dev._valid_smoke_report({"status": "passed", "audio_started": True}))
        good = {"status": "ready", "audio_started": False, "normal_window": True}
        self.assertTrue(dev._valid_startup_report({"ready": True}, good))
        self.assertFalse(dev._valid_startup_report({"ready": True}, {**good, "normal_window": False}))
        self.assertFalse(dev._valid_startup_report({"ready": False}, good))

    def test_portable_environment_removes_inherited_qt_and_audio_overrides(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder)
            keys = {"QT_PLUGIN_PATH":"sdk", "QML_IMPORT_PATH":"qml", "QT_QPA_PLATFORM_PLUGIN_PATH":"plugins",
                    "QT_QPA_PLATFORM":"offscreen", "QT_QUICK_BACKEND":"software", "WIRESCAPE_PORTAUDIO":"fake"}
            with patch.dict("os.environ", keys, clear=False):
                env = dev._portable_runtime_env(root)
            for key in keys:
                self.assertNotIn(key, env)
            self.assertEqual(env["PATH"].split(dev.os.pathsep)[0], str(root))

    def test_artifact_inventory_only_records_binary_artifacts(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder); build = root/'build'; build.mkdir()
            for name in ("native_engine_tests.exe", "native_engine_tests.lastbuildstate", "wirescape_cli", "ws_filter.dll", "notes.txt"):
                (build/name).write_bytes(b"x")
            with patch.object(dev, "BUILD_DIR", build):
                inventory = dev._artifact_inventory("Release")
            self.assertEqual(set(inventory), {"native_engine_tests.exe", "wirescape_cli", "ws_filter.dll"})

    def test_startup_test_requires_ready_audio_off_and_normal_window(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder); exe = root / "wirescape_studio.exe"; exe.touch()
            saved = {}
            def save(record): saved.update(record); return root / "startup.json"
            def readiness(command, cwd, log, report, env, timeout):
                report.write_text(json.dumps({"status":"ready", "audio_started":False,
                                              "normal_window":True, "platform":"windows"}))
                return {"ready":True, "exit_code":0, "log":str(log)}
            with patch.object(dev, "_base_record", return_value={"action":"startup-test"}), \
                 patch.object(dev, "_studio_executable", return_value=exe), \
                 patch.object(dev, "_save_record", side_effect=save), \
                 patch.object(dev, "wait_for_readiness", side_effect=readiness), \
                 patch.object(dev.os, "name", "nt"), patch.object(dev, "_qt_runtime_env", return_value={}):
                self.assertEqual(dev.command_startup_test(argparse.Namespace(config="Release", portable=None, json=True)), 0)
            self.assertEqual(saved["status"], "passed")
            self.assertFalse(saved["readiness"]["audio_started"])

    def test_ui_test_checks_both_requested_sizes_and_retains_reports(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder); exe = root / "wirescape_studio.exe"; exe.touch()
            counter = iter(range(10)); saved = {}
            def unique(stem, suffix): return root / f"{stem}-{next(counter)}{suffix}"
            def run(command, cwd, log, env, timeout):
                screenshot = Path(command[command.index("--screenshot") + 1]); screenshot.touch()
                report = Path(command[command.index("--report") + 1])
                width, height = map(int, command[command.index("--window-size") + 1].split("x"))
                report.write_text(json.dumps({"status":"passed", "logical_width":width,
                    "logical_height":height, "screenshot_saved":True, "visible_modules":4}))
                return {"exit_code":0, "log":str(log)}
            def save(record): saved.update(record); return root / "ui.json"
            with patch.object(dev, "_base_record", return_value={"action":"ui-test"}), \
                 patch.object(dev, "_studio_executable", return_value=exe), patch.object(dev, "_qt_runtime_env", return_value={}), \
                 patch.object(dev, "_unique_report_path", side_effect=unique), patch.object(dev, "run_logged", side_effect=run), \
                 patch.object(dev, "_save_record", side_effect=save):
                self.assertEqual(dev.command_ui_test(argparse.Namespace(config="Release", json=True)), 0)
            self.assertEqual([(x["ui_test"]["logical_width"], x["ui_test"]["logical_height"]) for x in saved["layouts"]],
                             [(1400, 900), (1000, 650)])
            self.assertEqual(saved["ui_test"]["visible_modules"], 4)

    def test_check_stops_after_first_failure_and_marks_later_stages_blocked(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root=Path(folder)
            def save(record):
                path=root/'check.json'; path.write_text(json.dumps(record)); return path
            with patch.object(dev, 'REPORT_DIR', root), patch.object(dev, '_base_record', return_value={'action':'check'}), \
                 patch.object(dev, '_save_record', side_effect=save), patch.object(dev, '_unique_report_path', side_effect=lambda stem,suffix: root/(stem+suffix)), \
                 patch.object(dev.unittest.defaultTestLoader, 'discover', return_value=unittest.TestSuite()), \
                 patch.object(dev, 'command_build', return_value=7) as build, \
                 patch.object(dev, 'command_test') as test, patch.object(dev, 'command_ui_test'), \
                 patch.object(dev, 'command_benchmark'), patch.object(dev, 'command_render'):
                code=dev.command_check(argparse.Namespace(config='Release', package=False, json=True))
            self.assertEqual(code, 7)
            self.assertEqual(build.call_count, 1)
            test.assert_not_called()
            result=json.loads((root/'check.json').read_text())
            self.assertEqual(result['stages'][1]['name'], 'build')
            self.assertEqual(result['stages'][2]['status'], 'blocked')

    def test_timeout_is_failure_and_keeps_log(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder)
            result = run_logged([sys.executable, '-c', 'import time; print("before", flush=True); time.sleep(5)'],
                                root, root/'timeout.log', timeout=.2)
            self.assertEqual(result['exit_code'], 124)
            self.assertTrue(result['timed_out'])
            self.assertIn('before', (root/'timeout.log').read_text())

    def test_report_excludes_summaries_and_keeps_latest_per_source(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder)
            for index, (action, source) in enumerate([('build', 'a'), ('build', 'a'), ('build', 'b'), ('report','a')]):
                (root/f'{index}.json').write_text(json.dumps({'action':action, 'status':'passed', 'source':{'sha256':source}}))
            with patch.object(dev, 'REPORT_DIR', root):
                records = dev._latest_reports()
            self.assertEqual(len(records), 2)
            self.assertTrue(all(r['action'] == 'build' for r in records))

    def test_invalid_explicit_qt_never_falls_back(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder)
            (root/'.tools/qt/6.10.3/msvc2022_64/lib/cmake/Qt6').mkdir(parents=True)
            (root/'.tools/qt/6.10.3/msvc2022_64/lib/cmake/Qt6/Qt6Config.cmake').touch()
            with patch.object(dev, 'ROOT', root):
                self.assertIsNotNone(dev._qt_prefix(None))
                self.assertIsNone(dev._qt_prefix(str(root/'missing')))

    def test_runtime_uses_recorded_sdk(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder)
            (root/'plugins').mkdir(); (root/'qml').mkdir()
            with patch.object(dev, '_build_metadata', return_value={'qt_prefix':str(root)}):
                env = dev._qt_runtime_env(root/'app/wirescape_studio.exe')
            self.assertEqual(env['QT_PLUGIN_PATH'], str(root/'plugins'))

    def test_fingerprint_tracks_untracked_source_and_excludes_caches(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder); (root/'native').mkdir(); (root/'tools/__pycache__').mkdir(parents=True)
            source = root/'native/module.cpp'; source.write_text('one')
            a = source_provenance(root)
            (root/'tools/__pycache__/a.pyc').write_bytes(b'cache')
            self.assertEqual(a['sha256'], source_provenance(root)['sha256'])
            source.write_text('two')
            self.assertNotEqual(a['sha256'], source_provenance(root)['sha256'])

    def test_cached_qt_and_flat_artifacts_refresh(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder); build = root/'build'; build.mkdir()
            qt = root/'sdk/lib/cmake/Qt6'; qt.mkdir(parents=True)
            (build/'CMakeCache.txt').write_text(f"Qt6_DIR:PATH={qt}\nCMAKE_BUILD_TYPE:STRING=Release\n")
            (build/'wirescape_cli').write_bytes(b"cli")
            (build/'libwirescape_engine.so').write_bytes(b"engine")
            (build/'libws_mixer.so').write_bytes(b"module")
            (build/'native_engine_tests').write_bytes(b"test")
            native = root/'native'; native.mkdir(); (native/'a.cpp').write_text('source')
            with patch.object(dev, 'ROOT', root), patch.object(dev, 'BUILD_DIR', build):
                metadata, stable = dev._refresh_build_metadata('Release', source_provenance(root))
            self.assertTrue(stable)
            self.assertEqual(Path(metadata['qt_prefix']), root/'sdk')
            self.assertIn('wirescape_cli', metadata['binaries'])
            self.assertIn('libwirescape_engine.so', metadata['binaries'])
            self.assertIn('libws_mixer.so', metadata['binaries'])
            self.assertIn('native_engine_tests', metadata['binaries'])

    def test_cached_qt_alternate_path_and_source_change_detection(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as folder:
            root = Path(folder); build = root/'build'; build.mkdir()
            qt = root/'alternate/Qt/6/lib/cmake/Qt6'; qt.mkdir(parents=True)
            (build/'CMakeCache.txt').write_text(f"Qt6_DIR:PATH={qt}\n")
            (root/'native').mkdir(); source=root/'native/a.cpp'; source.write_text('before')
            before=source_provenance(root); source.write_text('during')
            with patch.object(dev, 'ROOT', root), patch.object(dev, 'BUILD_DIR', build):
                metadata, stable=dev._refresh_build_metadata('Debug', before)
            self.assertFalse(stable)
            self.assertEqual(Path(metadata['qt_prefix']), root/'alternate/Qt/6')


if __name__ == '__main__':
    unittest.main()
