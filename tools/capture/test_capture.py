# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic and offline engine self-tests. Never launches a capture executable."""
import argparse
import json
from pathlib import Path
import unittest
from analysis import analyse, pilot, resample, stimulus
from reference import render, tone_patch


class AnalysisTests(unittest.TestCase):
    def setUp(self):
        self.reference = pilot(stimulus('pulses', 1))

    def test_clean_capture_has_latency_and_passes(self):
        report = analyse(self.reference, [0.0] * 4800 + self.reference + [0.0] * 4800)
        self.assertEqual(report['status'], 'pass', report)
        self.assertAlmostEqual(report['metrics']['latency_ms'], 100)

    def test_silence_is_detected(self):
        self.assertIn('silence', analyse(self.reference, [0.0] * len(self.reference))['failures'])

    def test_clipping_is_detected(self):
        x = self.reference.copy()
        x[5000:5500] = [1.0] * 500
        self.assertIn('clipping', analyse(self.reference, x)['failures'])

    def test_mismatch_is_detected(self):
        self.assertIn('mismatch', analyse(self.reference, pilot(stimulus('sweep', 1)))['failures'])

    def test_dropout_is_detected(self):
        x = self.reference.copy()
        x[5000:7500] = [0.0] * 2500
        result = analyse(self.reference, x)
        self.assertIn('dropout', result['failures'])
        self.assertGreater(result['metrics']['dropout_windows'], 0)

    def test_discontinuity_is_detected(self):
        x = self.reference.copy()
        x[5000] += 0.1
        self.assertIn('discontinuity', analyse(self.reference, x)['failures'])

    def test_truncated_capture_fails(self):
        self.assertIn('dropout', analyse(self.reference, self.reference[:-5000])['failures'])

    def test_sample_rate_conversion_and_gain(self):
        x = [0.0] * 4410 + resample(self.reference, 48000, 44100) + [0.0] * 4410
        result = analyse(self.reference, [v * 0.8 for v in x], 44100)
        self.assertEqual(result['status'], 'pass', result)
        self.assertTrue(result['metrics']['resampled'])

    def test_audio_outside_reference_is_detected(self):
        result = analyse(self.reference, self.reference + [0.02] * 4800)
        self.assertIn('unexpected_audio', result['failures'])

    def test_shifted_envelope_edges_are_detected(self):
        x = self.reference[:1280] + [0.0] * 2000 + self.reference[1280:]
        self.assertIn('timing', analyse(self.reference, x)['failures'])

    def test_bounds_and_nonfinite(self):
        for x in ([], [float('nan')] * 2000):
            with self.assertRaises(ValueError):
                analyse(self.reference, x)
        with self.assertRaises(ValueError):
            stimulus('tone', 100)


class EngineTests(unittest.TestCase):
    def test_native_patch_render_is_deterministic_and_responds_to_events(self):
        patch = tone_patch()
        a, _ = render(ARGS.engine, ARGS.modules, patch, seconds=0.2)
        b, _ = render(ARGS.engine, ARGS.modules, patch, seconds=0.2)
        self.assertEqual(a, b)
        self.assertGreater(max(a), 0.01)
        c, _ = render(ARGS.engine, ARGS.modules, patch,
                      [dict(frame=4800, node=1, type=1, parameter=0, value=880)], seconds=0.2)
        self.assertEqual(a[:4800], c[:4800])
        self.assertNotEqual(a[6000:], c[6000:])

    def test_negative_event_frame_is_rejected(self):
        with self.assertRaises(ValueError):
            render(ARGS.engine, ARGS.modules, tone_patch(),
                   [dict(frame=-1, node=1, type=1, value=440)], seconds=0.1)

    def test_filter_patch_attenuates_known_tone(self):
        patch = tone_patch()
        clean, _ = render(ARGS.engine, ARGS.modules, patch, seconds=0.2)
        patch['modules'].append(dict(id=3, type='essentials.vcf', values={'0': 100, '1': 0.2}))
        patch['connections'] = [dict(fromNode=1, fromPort='audio', toNode=3, toPort='audio'),
                                dict(fromNode=3, fromPort='low24', toNode=2, toPort='audio')]
        filtered, _ = render(ARGS.engine, ARGS.modules, patch, seconds=0.2)
        self.assertLess(sum(v * v for v in filtered[4800:]), sum(v * v for v in clean[4800:]) * 0.05)

    def test_combined_noise_demo_is_reproducible(self):
        path = Path(__file__).resolve().parents[2] / 'native/examples/Six_Essentials_Demo.json'
        patch = json.loads(path.read_text())
        a, _ = render(ARGS.engine, ARGS.modules, patch, seconds=0.2)
        b, _ = render(ARGS.engine, ARGS.modules, patch, seconds=0.2)
        self.assertEqual(a, b)
        self.assertGreater(max(map(abs, a)), 1e-5)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--engine', required=True)
    parser.add_argument('--modules', required=True)
    ARGS, rest = parser.parse_known_args()
    unittest.main(argv=[__file__] + rest, verbosity=2)
