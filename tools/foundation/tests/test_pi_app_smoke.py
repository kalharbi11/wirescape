# SPDX-License-Identifier: GPL-3.0-only
"""Mocked Pi app smoke: architecture, isolation, failure, timeout and cleanup."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location('pi_app_smoke', ROOT / 'tools/pi/app_smoke.py')
smoke = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(smoke)
SHA = '1234567890abcdef1234567890abcdef12345678'
CHECK_ENVIRONMENT = smoke.check_environment


class PiAppSmokeTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory(prefix='pi smoke fixture with spaces ')
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.report = self.root / 'build_reports/pi-app-smoke'
        self.report.mkdir(parents=True)
        self.objects = {
            name: self.root / name for name in ('engine', 'electron', 'main', 'ui', 'playwright')
        }
        for file in self.objects.values():
            file.write_bytes(b'test')
        patches = [
            patch.object(smoke, 'ROOT', self.root),
            patch.object(smoke, 'REQUIRED', self.objects),
            patch.object(smoke, 'git_sha', return_value=SHA),
            patch.object(smoke, 'check_environment', return_value=([], 'x11')),
            patch.object(smoke.shutil, 'which', return_value='/usr/bin/node'),
        ]
        for p in patches:
            p.start()
            self.addCleanup(p.stop)

    def test_arm64_elf_rejects_mismatched_and_missing_artifacts(self):
        file = self.objects['engine']
        header = bytearray(20)
        header[:6] = b'\x7fELF\x02\x01'
        header[18:20] = (183).to_bytes(2, 'little')
        file.write_bytes(header)
        self.assertTrue(smoke.arm64_elf(file))
        header[18:20] = (62).to_bytes(2, 'little')
        file.write_bytes(header)
        self.assertFalse(smoke.arm64_elf(file))
        # Incomplete/corrupt binary downloads must never raise IndexError.
        for length in (0, 4, 5, 6, 19):
            file.write_bytes(b'\x7fELF' + b'\x00' * max(0, length - 4)
                             if length else b'')
            self.assertFalse(smoke.arm64_elf(file), f'truncated ELF length={length}')
        file.unlink()
        self.assertFalse(smoke.arm64_elf(file))

    def test_short_elf_writes_blocked_summary_instead_of_crashing(self):
        self.objects['engine'].write_bytes(b'\x7fELF')
        # Simulate a fully provisioned Pi without starting any real process.
        with patch.object(smoke, 'check_environment', side_effect=CHECK_ENVIRONMENT), \
             patch.object(smoke.platform, 'system', return_value='Linux'), \
             patch.object(smoke.platform, 'machine', return_value='aarch64'), \
             patch.object(smoke, 'clean_checkout', return_value=True), \
             patch.object(smoke.ctypes.util, 'find_library', return_value='portaudio'), \
             patch.object(smoke, 'tool_versions', return_value={}), \
             patch.object(smoke, 'run_child', return_value=(0, 'mock build')), \
             patch.object(smoke.subprocess, 'run', return_value=
                          __import__('subprocess').CompletedProcess([], 0, '', '')), \
             patch.dict(os.environ, {'DISPLAY': ':99'}), \
             patch.object(sys, 'argv', ['app_smoke.py', '--report', str(self.report)]):
            code = smoke.main()
        self.assertEqual(code, 2)
        summary = json.loads((self.report / 'summary.json').read_text())
        self.assertEqual(summary['status'], 'blocked')
        self.assertTrue(any('engine is not a Linux ARM64 ELF64 artifact' in msg
                            for msg in summary['reason']))

    def test_wrong_platform_no_display_missing_artifacts_are_blocked(self):
        with patch.object(smoke, 'check_environment', side_effect=CHECK_ENVIRONMENT) as checker, \
             patch.object(smoke.platform, 'system', return_value='Linux'), \
             patch.object(smoke.platform, 'machine', return_value='x86_64'), \
             patch.object(smoke, 'clean_checkout', return_value=True), \
             patch.object(smoke.shutil, 'which', side_effect=lambda name: None if name == 'xvfb-run' else '/usr/bin/' + name), \
             patch.dict(os.environ, {'DISPLAY': '', 'WAYLAND_DISPLAY': ''}):
            issues, backend = checker()
        self.assertIsNone(backend)
        self.assertTrue(any('ARM64' in problem for problem in issues))
        self.assertTrue(any('DISPLAY' in problem for problem in issues))

    def test_existing_xvfb_is_a_functional_display_backend(self):
        with patch.object(smoke, 'check_environment', side_effect=CHECK_ENVIRONMENT), \
             patch.object(smoke.platform, 'system', return_value='Linux'), \
             patch.object(smoke.platform, 'machine', return_value='aarch64'), \
             patch.object(smoke, 'clean_checkout', return_value=True), \
             patch.object(smoke.ctypes.util, 'find_library', return_value='portaudio'), \
             patch.dict(os.environ, {'DISPLAY': '', 'WAYLAND_DISPLAY': ''}):
            problems, backend = smoke.check_environment(artifacts=False)
        self.assertEqual(problems, [])
        self.assertEqual(backend, 'xvfb')

    def test_xvfb_wraps_only_the_owned_real_app_child(self):
        calls = []
        def fake_run(command, cwd, env, seconds):
            calls.append(command)
            if env.get('WIRESCAPE_PI_APP_SMOKE'):
                self.assertEqual(command[:2], ['/usr/bin/xvfb-run', '-a'])
                self.assertEqual(env['WIRESCAPE_PI_DISPLAY_BACKEND'], 'xvfb')
                return 0, json.dumps({'stats': {'expected': 1}})
            self.assertNotIn('xvfb-run', command)
            return 0, 'mock build'
        with patch.object(smoke, 'check_environment', return_value=([], 'xvfb')), \
             patch.object(smoke.shutil, 'which', side_effect=lambda name: '/usr/bin/' + name), \
             patch.object(smoke, 'run_child', side_effect=fake_run):
            code, result = smoke.run(self.report, 900)
        self.assertEqual(code, 0)
        self.assertEqual(result['display_backend'], 'xvfb')
        self.assertEqual(len(calls), 5)

    def test_success_real_test_count_profile_cleanup_and_source_provenance(self):
        created = []
        calls = []
        def fake_run(command, cwd, env, seconds):
            calls.append((command, cwd))
            if env.get('WIRESCAPE_PI_APP_SMOKE') == '1':
                profile = Path(env['WIRESCAPE_PI_PROFILE_DIR'])
                created.append(profile)
                profile.joinpath('userData').mkdir()
                return 0, json.dumps({'stats': {'expected': 1, 'skipped': 0, 'unexpected': 0, 'flaky': 0}})
            return 0, 'synthetic build'
        with patch.object(smoke, 'run_child', side_effect=fake_run):
            code, result = smoke.run(self.report, 900)
        self.assertEqual(code, 0)
        self.assertEqual(result['status'], 'passed')
        self.assertEqual(result['source_sha'], SHA)
        self.assertEqual(len(calls), 5)
        self.assertEqual(result['test_counts']['expected'], 1)
        self.assertTrue(created)
        self.assertFalse(created[0].exists())
        self.assertNotIn(str(created[0]), (self.report / 'playwright.log').read_text())

    def test_stage_failure_stops_without_launch(self):
        with patch.object(smoke, 'run_child', return_value=(7, 'token=private-word')) as cmd:
            code, result = smoke.run(self.report, 900)
        self.assertEqual(code, 1)
        self.assertEqual(result['status'], 'failed')
        self.assertEqual(cmd.call_count, 1)
        self.assertIn('token=<redacted>', (self.report / 'native-build.log').read_text())
        self.assertNotIn('private-word', (self.report / 'native-build.log').read_text())

    def test_skipped_and_empty_playwright_results_are_not_passes(self):
        for stats in ({'expected': 0, 'skipped': 1}, {'expected': 1, 'unexpected': 1}):
            def fake_run(command, cwd, env, seconds):
                return (0, json.dumps({'stats': stats})) if env.get('WIRESCAPE_PI_APP_SMOKE') else (0, '')
            with self.subTest(stats=stats), patch.object(smoke, 'run_child', side_effect=fake_run):
                code, result = smoke.run(self.report, 900)
                self.assertEqual(code, 1)
                self.assertEqual(result['status'], 'failed')

    def test_missing_display_never_runs_a_child(self):
        with patch.object(smoke, 'check_environment', return_value=(['No display'], None)), \
             patch.object(smoke, 'run_child') as child:
            code, result = smoke.run(self.report, 900)
        self.assertEqual(code, 2)
        self.assertEqual(result['status'], 'blocked')
        child.assert_not_called()

    @unittest.skipIf(os.name == 'nt', 'Pi cleanup uses POSIX process groups')
    def test_timeout_kills_only_owned_process_group(self):
        cmd = [sys.executable, '-c', 'import time; time.sleep(4)']
        code, output = smoke.run_child(cmd, self.root, os.environ.copy(), 1)
        self.assertEqual(code, 124)
        self.assertIn('timed out', output)


if __name__ == '__main__':
    unittest.main()
