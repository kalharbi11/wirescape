# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import shlex
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from module_cpu_probe import parse_args, parse_line, validate_result


class ModuleCpuProbeTests(unittest.TestCase):
    def evidence(self, **changes):
        result = dict(module='sound.blend', instances=2, sample_rate=48000, block=128, seconds=60,
                      channels=1, blocks=22500, render_ns=dict(max=200000, p99=40000), period_ns=2666667,
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
                        dict(sample_rate=44100), dict(seconds=1), dict(blocks=0), dict(blocks=1), dict(blocks=22499), dict(peak=0),
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

    def test_invalid_duration(self):
        for seconds in ['nan', 'inf', '0.0001']:
            with self.subTest(seconds=seconds), self.assertRaises(SystemExit):
                parse_args(['sound.blend', '--seconds', seconds])

    def test_requested_polyphony(self):
        self.assertEqual(parse_args(['essentials.vco', '--channels', '8']).channels, 8)
        self.assertEqual(parse_args(['essentials.vco']).channels, 1)
        validate_result(self.evidence(channels=8), 'sound.blend', 60, 8)
        for channels in (1, 16):
            with self.subTest(channels=channels), self.assertRaises(ValueError):
                validate_result(self.evidence(channels=channels), 'sound.blend', 60, 8)
        for channels in ('0', '17'):
            with self.subTest(channels=channels), self.assertRaises(SystemExit):
                parse_args(['essentials.vco', '--channels', channels])
        result = self.evidence()
        del result['channels']
        with self.assertRaises(ValueError):
            validate_result(result, 'sound.blend', 60)

    def test_worst_case_settings(self):
        args = parse_args(['sound.blend', '--param', 'balance=0.25', '--param', 'fade=0.1',
                           '--alternate', 'mode=0:1:4'])
        self.assertEqual(args.param, ['balance=0.25', 'fade=0.1'])
        self.assertEqual(args.alternate, 'mode=0:1:4')
        for bad in (['--param', 'balance'], ['--param', '=0.25'], ['--alternate', 'mode=0:1'], ['--alternate', 'mode:0:1:2']):
            with self.subTest(bad=bad), self.assertRaises(SystemExit):
                parse_args(['sound.blend', *bad])


    def test_spectral_pi_stress_reaches_both_modes_on_actual_hops(self):
        # At 48k/128 and size=3, each 4096/4-sample Spectral hop spans 8 blocks.
        # The probe sets mode before every block; tick() reads it on each hop's
        # last sample. Inspect the checked-in acceptance command directly.
        task = Path(__file__).resolve().parents[3] / 'docs/tasks/done/S1_1_SPECTRAL.md'
        lines = [line.strip() for line in task.read_text(encoding='utf-8').splitlines()
                 if line.strip().startswith('- pi-run:') and '--alternate mode=' in line]
        self.assertEqual(len(lines), 1)
        args = parse_args(shlex.split(lines[0].split('tools/module_cpu_probe.py', 1)[1]))
        self.assertEqual(args.module, 'sound.spectral')
        self.assertIn('size=3', args.param)
        self.assertIn('pitch=1', args.param)
        control, values = args.alternate.split('=', 1)
        first, second, every = values.split(':')
        self.assertEqual((control, first, second), ('mode', '0', '1'))
        every = int(every)
        blocks_per_hop = (4096 // 4) // 128
        sampled = [((hop * blocks_per_hop - 1) // every) % 2 for hop in range(1, 17)]
        self.assertEqual(sampled, [0] * 4 + [1] * 4 + [0] * 4 + [1] * 4)
        # Four-block toggles alias against eight-block hops: one sampled mode.
        self.assertEqual(len({((hop * blocks_per_hop - 1) // 4) % 2
                              for hop in range(1, 17)}), 1)


if __name__ == '__main__':
    unittest.main()
