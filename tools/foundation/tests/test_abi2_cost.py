# SPDX-License-Identifier: GPL-3.0-only
"""Cost collection must use Ir, propagate failures, and subtract startup work."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('abi2_cost', Path(__file__).resolve().parents[2] / 'bench/abi2_cost.py')
cost = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cost)


class CostTests(unittest.TestCase):
    def test_difference_uses_ir_column_and_cancels_fixed_cost(self):
        def fake_run(command, **kwargs):
            output = next(x.split('=', 1)[1] for x in command if x.startswith('--callgrind-out-file='))
            blocks = int(command[-1])
            Path(output).write_text(f'events: Dr Ir\nsummary: 99 {7000 + 123 * blocks}\n')
        with patch.object(cost.subprocess, 'run', side_effect=fake_run):
            row = cost.cost(Path('probe'), Path('modules'), 'abi2_mono', (10, 30), 'valgrind')
        self.assertEqual(row['ir_per_block'], 123)

    def test_probe_failure_is_not_a_skipped_or_passing_measurement(self):
        with patch.object(cost.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'probe')):
            with self.assertRaises(subprocess.CalledProcessError):
                cost.cost(Path('probe'), Path('modules'), 'abi2_mono', (10, 30), 'valgrind')

    def test_missing_ir_summary_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            file = Path(folder) / 'callgrind'
            file.write_text('events: Dr\nsummary: 99\n')
            with self.assertRaises(ValueError):
                cost.instructions(file)

    def test_valgrind_absent_is_explicitly_skipped(self):
        with patch.object(cost.shutil, 'which', return_value=None):
            self.assertEqual(cost.main([]), 0)
