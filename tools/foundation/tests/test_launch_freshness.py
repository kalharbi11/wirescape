# SPDX-License-Identifier: GPL-3.0-only
"""Electron launch provenance/consent regressions; no real app or device is started."""
import argparse
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import dev  # noqa: E402


class LaunchFreshnessTests(unittest.TestCase):
    def tree(self, root):
        build = root / 'build'
        engine = build / 'sidecar' / ('wirescape-engine.exe' if os.name == 'nt' else 'wirescape-engine')
        files = [engine, root/'native/source.cpp', root/'apps/desktop/main.ts',
                 root/'apps/studio/src/App.ts', root/'apps/desktop/build/main.js',
                 root/'apps/desktop/build/preload.js', root/'apps/studio/dist/index.html',
                 root/'apps/desktop/node_modules/electron/dist/electron']
        for file in files:
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b'verified')
        (root/'apps/desktop/node_modules/electron/path.txt').write_text('electron')
        return build, engine

    def certify(self):
        metadata, stable = dev._refresh_build_metadata('Release', dev.source_provenance(dev.ROOT),
                                                      True, dev._desktop_inventory())
        self.assertTrue(stable)
        self.assertTrue(metadata['desktop_verified'])
        return metadata

    def test_launch_requires_matching_desktop_and_engine_evidence(self):
        for change in ('matching', 'native_source', 'desktop_source', 'engine', 'artifact', 'electron',
                       'missing', 'corrupt', 'config', 'headless', 'unstable', 'missing_entry', 'missing_digest'):
            with self.subTest(change=change), tempfile.TemporaryDirectory(prefix='electron launch ') as folder:
                root = Path(folder)
                build, engine = self.tree(root)
                records = []
                with patch.object(dev, 'ROOT', root), patch.object(dev, 'BUILD_DIR', build):
                    metadata = self.certify()
                    if change == 'native_source':
                        (root/'native/source.cpp').write_bytes(b'changed')
                    elif change == 'desktop_source':
                        (root/'apps/desktop/main.ts').write_bytes(b'changed')
                    elif change == 'engine':
                        engine.write_bytes(b'changed')
                    elif change == 'artifact':
                        (root/'apps/desktop/build/main.js').write_bytes(b'changed')
                    elif change == 'electron':
                        dev._electron_executable().write_bytes(b'changed')
                    elif change == 'missing_entry':
                        (root/'apps/desktop/build/preload.js').unlink()
                    elif change in ('config', 'headless', 'unstable', 'missing_digest'):
                        if change == 'config':
                            metadata['configuration'] = 'Debug'
                        elif change == 'headless':
                            metadata['desktop_verified'] = False
                        elif change == 'unstable':
                            metadata['source_stable'] = False
                        else:
                            metadata['binaries'] = {}
                        (build/'build-metadata.json').write_text(json.dumps(metadata))
                    elif change == 'missing':
                        (build/'build-metadata.json').unlink()
                    elif change == 'corrupt':
                        (build/'build-metadata.json').write_text('not json')
                    # Mock only the GUI child; leave provenance's git subprocess real.
                    original = dev.subprocess.Popen
                    def child(command, **kwargs):
                        if command[0] == str(dev._electron_executable()):
                            return SimpleNamespace(pid=42, poll=lambda: None)
                        return original(command, **kwargs)
                    with patch.object(dev, 'REPORT_DIR', root/'reports'), \
                         patch.object(dev, '_base_record', return_value={'action': 'launch'}), \
                         patch.object(dev, '_save_record', side_effect=lambda record: records.append(record) or root/'report'), \
                         patch.object(dev, '_print_record'), patch.object(dev.subprocess, 'Popen', side_effect=child) as spawn:
                        code = dev.command_launch(dev.make_parser().parse_args(['launch']))
                    self.assertEqual(code, 0 if change == 'matching' else 2)
                    gui = [call for call in spawn.call_args_list if call.args[0][0] != 'git']
                    self.assertEqual(len(gui), 1 if change == 'matching' else 0)
                    if change != 'matching':
                        self.assertIn('python tools/dev.py build --config Release', records[-1]['summary'])
                    else:
                        self.assertEqual(records[-1]['status'], 'started')  # Not a GUI-readiness claim.

    def test_all_four_device_consent_combinations_use_separate_argv(self):
        for flags in ([], ['--confirm-audio'], ['--confirm-midi'], ['--confirm-audio', '--confirm-midi']):
            with self.subTest(flags=flags), tempfile.TemporaryDirectory(prefix='electron with spaces ') as folder:
                root = Path(folder)
                build, engine = self.tree(root)
                with patch.object(dev, 'ROOT', root), patch.object(dev, 'BUILD_DIR', build):
                    self.certify()
                    with patch.object(dev, '_launch_freshness_error', return_value=None), \
                         patch.object(dev, '_base_record', return_value={'action': 'launch'}), \
                         patch.object(dev, 'REPORT_DIR', root/'reports'), patch.object(dev, '_print_record'), \
                         patch.object(dev.subprocess, 'Popen', return_value=SimpleNamespace(pid=42, poll=lambda: None)) as spawn:
                        self.assertEqual(dev.command_launch(dev.make_parser().parse_args(['launch', *flags])), 0)
                    argv = spawn.call_args.args[0]
                    self.assertEqual(argv[:4], [str(dev._electron_executable()), str(root/'apps/desktop'), '--engine', str(engine)])
                    self.assertEqual(argv[4:], flags or ['--null-audio'])
                    self.assertFalse(spawn.call_args.kwargs.get('shell', False))

    def test_failed_build_invalidates_previous_desktop_certificate(self):
        for results in ([{'exit_code': 7}], [{'exit_code': 0}, {'exit_code': 7}],
                        [{'exit_code': 0}, {'exit_code': 0}, {'exit_code': 7}]):
            with self.subTest(stage=len(results)), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                build, _ = self.tree(root)
                (root/'native/CMakeLists.txt').touch()
                metadata = build/'build-metadata.json'
                metadata.write_text(json.dumps({'desktop_verified': True}))
                with patch.object(dev, 'ROOT', root), patch.object(dev, 'BUILD_DIR', build), \
                     patch.object(dev, '_base_record', return_value={'source': {}}), \
                     patch.object(dev, '_cmake_and_generator', return_value=('cmake', 'Ninja', 'x64')), \
                     patch.object(dev, '_node_npm', return_value=('node', 'npm-cli.js')), \
                     patch.object(dev, 'run_logged', side_effect=results), \
                     patch.object(dev, '_save_record'), patch.object(dev, '_print_record'):
                    self.assertEqual(dev.command_build(argparse.Namespace(config='Release', headless=False, json=False)), 7)
                self.assertIs(json.loads(metadata.read_text())['desktop_verified'], False)

    def test_metadata_refresh_cannot_certify_leftover_or_unstable_desktop(self):
        for built, missing, changed in ((False, False, False), (True, True, False), (True, False, True), (True, False, False)):
            with self.subTest(built=built, missing=missing, changed=changed), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                build, _ = self.tree(root)
                with patch.object(dev, 'ROOT', root), patch.object(dev, 'BUILD_DIR', build):
                    before = dev._desktop_inventory()
                    if missing:
                        (root/'apps/desktop/build/main.js').unlink()
                    if changed:
                        (root/'apps/desktop/main.ts').write_bytes(b'changed')
                    metadata, stable = dev._refresh_build_metadata('Release', dev.source_provenance(root), built, before)
                self.assertIs(stable, not changed)
                self.assertIs(metadata['desktop_verified'], built and not missing and not changed)

    def test_test_build_preserves_only_an_unchanged_desktop_certificate(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            build, engine = self.tree(root)
            with patch.object(dev, 'ROOT', root), patch.object(dev, 'BUILD_DIR', build):
                self.certify()
                self.assertTrue(dev._refresh_build_metadata('Release')[0]['desktop_verified'])
                engine.write_bytes(b'changed')
                self.assertFalse(dev._refresh_build_metadata('Release')[0]['desktop_verified'])

    def test_retired_qt_commands_are_not_accepted(self):
        with contextlib.redirect_stderr(io.StringIO()):
            for command in ('ui-test', 'startup-test', 'package', 'updates'):
                with self.assertRaises(SystemExit):
                    dev.make_parser().parse_args([command])


if __name__ == '__main__':
    unittest.main()
