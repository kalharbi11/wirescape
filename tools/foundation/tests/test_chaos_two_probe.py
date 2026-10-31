# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from chaos_two_probe import validate_result


class ChaosProbeTests(unittest.TestCase):
    def evidence(self, **changes):
        result = dict(sample_rate=48000, block=128, seconds=120, blocks=45000,
                      allocations=0, faults=0, over_100_percent=0, peak=.2,
                      render_ns=dict(max=1000000, p99=100))
        result.update(changes)
        return result

    def test_valid_evidence_passes(self):
        validate_result(self.evidence())

    def test_good_p99_does_not_hide_bad_worst_block(self):
        with self.assertRaises(ValueError):
            validate_result(self.evidence(render_ns=dict(max=1e9 * 128 / 48000 * .5, p99=100)))

    def test_incomplete_wrong_format_or_runtime_faults_fail(self):
        for changes in (dict(faults=1), dict(allocations=1), dict(seconds=1), dict(block=64),
                        dict(sample_rate=44100), dict(peak=0), dict(peak=float('nan')),
                        dict(render_ns=dict(max=float('nan'))), dict(render_ns=dict(max=-1)),
                        dict(render_ns={}), dict(over_100_percent=1)):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                validate_result(self.evidence(**changes))
