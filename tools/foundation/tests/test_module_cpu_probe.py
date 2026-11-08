# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from module_cpu_probe import parse_args, parse_line, validate_result


class ModuleCpuProbeTests(unittest.TestCase):
    def evidence(self, **changes):
        result = dict(module='sound.blend', instances=2, sample_rate=48000, block=128, seconds=60,
                      blocks=22500, render_ns=dict(max=200000, p99=40000), period_ns=2666667,
                      allocations=0, faults=0, peak=.4)
        result.update(changes)
        return result

    def test_valid_evidence_passes(self):
        validate_result(self.evidence(), 'sound.blend', 60)

    def test_worst_block_at_half_period_fails(self):
        with self.assertRaises(ValueError):
            validate_result(self.evidence(render_ns=dict(max=1e9 * 128 / 48000 * .5, p99=10)), 'sound.blend', 60)

    def test_wrong_setup_or_runtime_faults_fail(self):
        for changes in (dict(allocations=1), dict(faults=1), dict(instances=1), dict(block=64),
                        dict(sample_rate=44100), dict(seconds=1), dict(blocks=0), dict(peak=0),
                        dict(peak=float('nan')), dict(module='essentials.noise'), dict(render_ns={}),
                        dict(render_ns=dict(max=100, p99=200)), dict(render_ns=dict(max=float('nan'), p99=1))):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                validate_result(self.evidence(**changes), 'sound.blend', 60)

    def test_output_line_is_found_and_parsed(self):
        line = 'noise\nmodule_cpu_probe {"module":"sound.blend","render_ns":{"max":5,"p99":3}}\n'
        self.assertEqual(parse_line(line)['render_ns']['p99'], 3)
        with self.assertRaises(ValueError):
            parse_line('nothing here\n')

    def test_arguments(self):
        args = parse_args(['sound.blend', '--seconds', '5'])
        self.assertEqual((args.module, args.seconds), ('sound.blend', 5.0))
        self.assertEqual(parse_args(['essentials.noise']).seconds, 60.0)
        with self.assertRaises(SystemExit):
            parse_args(['sound.blend', '--seconds', '0'])
        with self.assertRaises(SystemExit):
            parse_args([])

    def test_worst_case_settings(self):
        args = parse_args(['sound.blend', '--param', 'balance=0.25', '--param', 'fade=0.1',
                           '--alternate', 'mode=0:1:4'])
        self.assertEqual(args.param, ['balance=0.25', 'fade=0.1'])
        self.assertEqual(args.alternate, 'mode=0:1:4')
        for bad in (['--param', 'balance'], ['--param', '=0.25'], ['--alternate', 'mode=0:1'], ['--alternate', 'mode:0:1:2']):
            with self.subTest(bad=bad), self.assertRaises(SystemExit):
                parse_args(['sound.blend', *bad])


if __name__ == '__main__':
    unittest.main()
