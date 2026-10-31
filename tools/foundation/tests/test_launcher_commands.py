# SPDX-License-Identifier: GPL-3.0-only
"""Fake runtime and remote metadata tests; never start Studio or load a DLL."""
import argparse
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import dev


class LauncherCommandTests(unittest.TestCase):
    def test_remember_and_override_are_validated_and_reported(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            qt = root / 'qt'
            for relative in ('lib/cmake/Qt6/Qt6Config.cmake', 'bin/Qt6Core.dll',
                             'bin/Qt6Gui.dll', 'bin/Qt6Qml.dll', 'bin/Qt6Quick.dll',
                             'plugins/platforms/qwindows.dll'):
                path = qt / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            pa = root / 'libportaudio64bit.dll'
            pa.touch()
            args = argparse.Namespace(qt_prefix=str(qt), portaudio=str(pa), remember_runtime=True)
            with patch.dict(os.environ, {'LOCALAPPDATA': str(root)}), patch.object(dev, '_build_metadata', return_value={}):
                selected = dev._launcher_dependencies(args, root / 'studio.exe')
                self.assertEqual(selected['qt']['source'], 'explicit')
                args.qt_prefix = args.portaudio = None
                args.remember_runtime = False
                self.assertEqual(dev._launcher_dependencies(args, root / 'studio.exe')['qt']['source'], 'remembered')
                args.qt_prefix = str(root / 'missing')
                with self.assertRaises(ValueError):
                    dev._launcher_dependencies(args, root / 'studio.exe')
                stored = json.loads((root / 'Wirescape/launcher.json').read_text())
                self.assertEqual(stored['qt'], str(qt.resolve()))

    def test_missing_environment_candidates_do_not_hide_built_runtime(self):
        with tempfile.TemporaryDirectory() as folder:
            from foundation.launcher_runtime import choose_path
            pa = Path(folder) / 'libportaudio64bit.dll'
            pa.touch()
            selected = choose_path('portaudio', candidates=[('environment', None), ('missing', pa.parent / 'missing'), ('build', pa)])
            self.assertEqual(selected['source'], 'build')

    def test_update_check_is_read_only_and_compares_installed_not_checkout(self):
        installed, latest = 'a' * 40, 'b' * 40
        args = argparse.Namespace(config='Release', json=False, open_instructions=True)
        record = {'action': 'updates'}
        with patch.object(dev, '_base_record', return_value=record), \
             patch.object(dev, '_build_metadata', return_value={'build_commit': installed, 'source': {'git_dirty': False}}), \
             patch.object(dev, '_studio_executable', return_value=Path('studio.exe')), \
             patch.object(dev, '_launch_freshness_error', return_value=None), \
             patch.object(dev, 'git_revision', return_value='c' * 40), \
             patch.object(dev.webbrowser, 'open', return_value=True) as opener, \
             patch.object(dev, '_save_record', return_value=Path('unused')), \
             patch.object(dev.subprocess, 'run', side_effect=[subprocess.CompletedProcess([], 0, latest + '\trefs/heads/main\n'),
                          subprocess.CompletedProcess([], 0, '2026-10-29\n'),
                          subprocess.CompletedProcess([], 1, '')]) as run, \
             contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(dev.command_updates(args), 0)
        self.assertIn(record['summary'], output.getvalue())
        self.assertEqual(record['installed'], installed)
        self.assertEqual(record['comparison'], 'different')
        self.assertEqual(run.call_args_list[0].args[0], ['git', 'ls-remote', 'origin', 'refs/heads/main'])
        self.assertEqual(run.call_count, 3)
        self.assertEqual(record['latest_main'], latest)
        self.assertEqual(record['installed_commit_date'], '2026-10-29')
        self.assertEqual(record['latest_main_commit_date'], 'unavailable')
        self.assertIn('Installed aaaaaaa (commit date 2026-10-29)', record['summary'])
        self.assertIn('latest main bbbbbbb (commit date unavailable)', record['summary'])
        self.assertNotIn(installed, record['summary'])
        self.assertNotIn(latest, record['summary'])
        for call, sha in zip(run.call_args_list[1:], (installed, latest)):
            self.assertEqual(call.args[0], ['git', 'show', '--no-patch', '--format=%cs', sha])
        opener.assert_called_once_with((dev.ROOT / 'docs/WINDOWS_DEMO_UPDATE.md').as_uri())

    def test_dirty_installed_build_is_not_current(self):
        record = {}
        with patch.object(dev, '_base_record', return_value=record), \
             patch.object(dev, '_build_metadata', return_value={'build_commit': 'a' * 40, 'source': {'git_dirty': True}}), \
             patch.object(dev, 'git_revision', return_value='a' * 40), \
             patch.object(dev, '_save_record', return_value=Path('unused')), \
             patch.object(dev.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'a' * 40)), \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(dev.command_updates(argparse.Namespace(config='Release', json=True)), 0)
        self.assertEqual(record['comparison'], 'modified-or-unverified-build')

    def test_current_update_requires_actual_verified_gui(self):
        for change in ('matching', 'headless', 'unstable', 'config', 'missing',
                       'binary', 'source', 'ui_disabled', 'dirty'):
            with self.subTest(change=change), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                build = root / 'build'
                build.mkdir()
                executable = build / ('wirescape_studio.exe' if os.name == 'nt' else 'wirescape_studio')
                executable.write_bytes(b'verified GUI')
                sha = 'a' * 40
                metadata = {
                    'build_commit': sha, 'configuration': 'Release',
                    'ui_verified': True, 'source_stable': True,
                    'source': {'sha256': 'original', 'git_dirty': False},
                    'binaries': {executable.name: dev.sha256_file(executable)},
                }
                if change == 'headless':
                    metadata['ui_verified'] = False
                elif change == 'unstable':
                    metadata['source_stable'] = False
                elif change == 'config':
                    metadata['configuration'] = 'Debug'
                elif change == 'missing':
                    executable.unlink()
                elif change == 'binary':
                    executable.write_bytes(b'replaced GUI')
                elif change == 'dirty':
                    metadata['source']['git_dirty'] = True
                record = {}
                with patch.object(dev, 'ROOT', root), patch.object(dev, 'BUILD_DIR', build), \
                     patch.object(dev, '_base_record', return_value=record), \
                     patch.object(dev, '_build_metadata', return_value=metadata), \
                     patch.object(dev, '_cache_values', return_value={'WIRESCAPE_BUILD_UI': 'OFF' if change == 'ui_disabled' else 'ON'}), \
                     patch.object(dev, 'source_provenance', return_value={'sha256': 'changed' if change == 'source' else 'original'}), \
                     patch.object(dev, 'git_revision', return_value=sha), \
                     patch.object(dev, '_save_record', return_value=root / 'unused'), \
                     patch.object(dev.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, sha)) as run, \
                     patch.object(dev, 'wait_for_readiness') as launch, \
                     contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(dev.command_updates(argparse.Namespace(config='Release', json=True)), 0)
                expected = 'current' if change == 'matching' else 'modified-or-unverified-build'
                self.assertEqual(record['comparison'], expected)
                if change != 'matching':
                    self.assertTrue(record['installed_build_error'])
                    self.assertIn(record['installed_build_error'], record['summary'])
                launch.assert_not_called()
                self.assertEqual([call.args[0] for call in run.call_args_list], [
                    ['git', 'ls-remote', 'origin', 'refs/heads/main'],
                    ['git', 'show', '--no-patch', '--format=%cs', sha],
                    ['git', 'show', '--no-patch', '--format=%cs', sha],
                ])

    def test_update_failure_does_not_claim_current_or_update_checkout(self):
        for result in (subprocess.CompletedProcess([], 1, ''),
                       subprocess.CompletedProcess([], 0, 'not-a-sha\trefs/heads/main\n')):
            record = {}
            with patch.object(dev, '_base_record', return_value=record), \
                 patch.object(dev, '_build_metadata', return_value={'build_commit': 'a' * 40}), \
                 patch.object(dev, '_save_record', return_value=Path('unused')), \
                 patch.object(dev.subprocess, 'run', return_value=result), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(dev.command_updates(argparse.Namespace(config='Release', json=True)), 1)
                self.assertNotIn('comparison', record)

    def test_package_reports_invalid_runtime_without_loading_it(self):
        record = {'action': 'package'}
        with patch.object(dev, '_base_record', return_value=record), \
             patch.object(dev, '_studio_executable', return_value=Path('studio.exe')), \
             patch.object(dev, '_build_metadata', return_value={}), \
             patch.object(dev, 'os', SimpleNamespace(name='nt')), \
             patch.object(dev, '_launcher_dependencies', side_effect=ValueError('Invalid remembered Qt path\nmissing Qt6Core.dll')), \
             patch.object(dev, '_save_record', return_value=Path('unused')), \
             patch.object(dev.subprocess, 'run') as run, \
             contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(dev.command_package(argparse.Namespace(config='Release', json=False)), 1)
        self.assertIn('Runtime selection failed: Invalid remembered Qt path missing Qt6Core.dll', output.getvalue())
        run.assert_not_called()

    def test_cli_runtime_overrides_and_update_entry(self):
        args = dev.make_parser().parse_args(['launch', '--qt-prefix', 'qt', '--portaudio', 'pa', '--remember-runtime'])
        self.assertEqual((args.qt_prefix, args.portaudio, args.remember_runtime), ('qt', 'pa', True))
        self.assertIs(dev.make_parser().parse_args(['updates']).func, dev.command_updates)


if __name__ == '__main__':
    unittest.main()

