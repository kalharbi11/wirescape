# SPDX-License-Identifier: GPL-3.0-only
"""POSIX launcher dry-runs never execute CLI, Chromium, HTTP or device code."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipIf(os.name == 'nt', 'Pi launcher needs POSIX sh and /proc')
class PiDemoTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.home = self.root / 'home space'
        self.home.mkdir()
        self.script = self.root / 'tools/pi/demo.sh'
        self.script.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / 'tools/pi/demo.sh', self.script)
        self.examples = self.root / 'native/examples'
        self.examples.mkdir(parents=True)
        self.original = {'devices': [], 'modules': [], 'unknown': {'keep': True}}
        for i in range(1, 4):
            (self.examples / f'class_demo_{i}_test.json').write_text(json.dumps(self.original))
        config = self.home / '.config/wirescape/demo.env'
        config.parent.mkdir(parents=True)
        config.write_text("CLI='/fake path/cli'\nUI_DIST='/fake path/ui'\n"
                          "AUDIO_DEVICE='Audio with spaces'\n"
                          "MIDI_PORTS='Keys|dev:keys;Knobs $(false)|dev:knobs'\n"
                          "CHROMIUM='/fake path/chromium'\nBLOCK=512\n")

    def run_script(self, *args):
        return subprocess.run(['sh', str(self.script), *args],
                              env={**os.environ, 'HOME': str(self.home)},
                              text=True, capture_output=True, timeout=10)

    def test_all_patches_preserve_data_and_exact_argument_boundaries(self):
        for i in range(1, 4):
            result = self.run_script('--dry-run', str(i))
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(result.stdout)
            copy = self.home / f'.cache/wirescape/class_demo_{i}.json'
            self.assertEqual(report['patch_path'], str(copy))
            self.assertFalse(copy.exists())
            self.assertEqual(report['patch']['unknown'], {'keep': True})
            self.assertEqual(report['patch']['devices'], [
                dict(kind='midi', name='Keys', system_name='dev:keys', enabled=True),
                dict(kind='midi', name='Knobs $(false)', system_name='dev:knobs', enabled=True)])
            self.assertEqual(report['player'], ['/fake path/cli', 'play', str(copy),
                             '--confirm-audio', '--confirm-midi', '--device',
                             'Audio with spaces', '--rate', '48000', '--block', '512'])
            self.assertEqual(report['server'], ['python3', '-I', '-m', 'http.server',
                             '4173', '--bind', '127.0.0.1', '--directory', '/fake path/ui'])
            self.assertEqual(report['kiosk'], ['/fake path/chromium',
                             '--user-data-dir=' + str(self.home / '.cache/wirescape/chromium'),
                             '--kiosk', '--app=http://127.0.0.1:4173'])
            self.assertEqual(json.loads((self.examples / f'class_demo_{i}_test.json').read_text()), self.original)
        self.assertFalse((self.home / '.cache/wirescape/demo-pids.json').exists())

    def test_invalid_arguments_and_config_fail_before_launch(self):
        for args in [('4',), (), ('--dry-run', 'stop'), ('--dry-run', '1', '2')]:
            self.assertNotEqual(self.run_script(*args).returncode, 0)
        config = self.home / '.config/wirescape/demo.env'
        config.write_text("MIDI_PORTS='missing separator'\n")
        self.assertIn('name|system_name', self.run_script('--dry-run', '1').stderr)
        config.write_text('BLOCK=0\n')
        self.assertIn('BLOCK', self.run_script('--dry-run', '1').stderr)

    def test_stale_pid_is_not_signalled_and_status_and_stop_need_no_build(self):
        cache = self.home / '.cache/wirescape'
        cache.mkdir(parents=True)
        # Current test process is unrelated. A stale start token must prevent killing it.
        (cache / 'demo-pids.json').write_text(json.dumps({
            'player': {'pid': os.getpid(), 'start': 'stale-token'}}))
        status = self.run_script('status')
        self.assertEqual(status.returncode, 0, status.stderr)
        self.assertIn('player stopped', status.stdout)
        stopped = self.run_script('stop')
        self.assertEqual(stopped.returncode, 0, stopped.stderr)
        self.assertEqual(json.loads((cache / 'demo-pids.json').read_text()), {})


if __name__ == '__main__':
    unittest.main()
