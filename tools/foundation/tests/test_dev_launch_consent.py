# SPDX-License-Identifier: GPL-3.0-only
"""LAN launch consent is argv-only in tests; never start Electron or devices."""
from itertools import product
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import dev  # noqa: E402


class LaunchLanConsentTests(unittest.TestCase):
    def test_all_eight_consent_combinations_have_safe_argv_and_record(self):
        for audio, midi, lan in product((False, True), repeat=3):
            flags = ([*(['--confirm-audio'] if audio else []),
                      *(['--confirm-midi'] if midi else []),
                      *(['--confirm-lan'] if lan else [])])
            with self.subTest(flags=flags), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                engine, electron = root / 'engine', root / 'electron'
                recorded = {}
                with (
                    patch.object(dev, '_sidecar_executable', return_value=engine),
                    patch.object(dev, '_electron_executable', return_value=electron),
                    patch.object(dev, '_launch_freshness_error', return_value=None),
                    patch.object(dev, '_base_record', return_value={'action': 'launch'}),
                    patch.object(dev, '_unique_report_path', return_value=root / 'launch.log'),
                    patch.object(dev, '_save_record',
                                 side_effect=lambda r: recorded.update(r) or root / 'report'),
                    patch.object(dev, '_print_record'),
                    patch.object(dev.subprocess, 'Popen',
                                 return_value=SimpleNamespace(pid=42, poll=lambda: None)) as spawn,
                ):
                    args = dev.make_parser().parse_args(['launch', *flags])
                    self.assertEqual(dev.command_launch(args), 0)
                audio_midi = [f for f in flags if f != '--confirm-lan']
                expected = [str(electron), str(dev.ROOT / 'apps/desktop'),
                            '--engine', str(engine), *(audio_midi or ['--null-audio']),
                            *(['--confirm-lan'] if lan else [])]
                self.assertEqual(spawn.call_args.args[0], expected)
                self.assertEqual(recorded['command_result']['command'], expected)
                self.assertEqual(recorded['device_confirmations'], flags)
                self.assertIs(spawn.call_args.kwargs['stdin'], dev.subprocess.DEVNULL)
                self.assertFalse(spawn.call_args.kwargs.get('shell', False))

