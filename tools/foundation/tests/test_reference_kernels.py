import importlib.util
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).parents[2] / "experiments" / "reference_kernels.py"
SPEC = importlib.util.spec_from_file_location("reference_kernels", MODULE_PATH)
rk = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(rk)


class ReferenceKernelTests(unittest.TestCase):
    def test_mix_matches_numpy_in_place(self):
        a, b = [-1.0, 0.0, .125], [.5, -.25, .75]
        expected = [0.0] * 3
        rk.mix_py(a, b, .375, expected)
        self.assertEqual(expected, [a[i] * .375 + b[i] for i in range(3)])
        if rk.np is not None:
            actual = rk.np.zeros(3, dtype=rk.np.float64)
            rk.mix_np(rk.np.asarray(a), rk.np.asarray(b), .375, actual)
            for x, y in zip(expected, actual):
                self.assertAlmostEqual(x, float(y), places=15)

    def test_ramp_matches_numpy_and_excludes_endpoint(self):
        expected = [0.0] * 7
        rk.ramp_py(-.25, .75, expected)
        self.assertEqual(expected[0], -.25)
        self.assertLess(expected[-1], .75)
        if rk.np is not None:
            actual = rk.np.zeros(7, dtype=rk.np.float64)
            rk.ramp_np(-.25, .75, rk.np.arange(7, dtype=rk.np.float64), actual)
            for x, y in zip(expected, actual):
                self.assertAlmostEqual(x, float(y), places=15)

    def test_delay_reset_and_partition_equivalence(self):
        source = [float(i + 1) for i in range(211)]
        expected, state, cursor = [0.0] * len(source), [0.0] * rk.DELAY, 0
        cursor = rk.delay_py(source, state, cursor, expected)
        self.assertEqual(expected[:min(rk.DELAY, len(source))], [0.0] * min(rk.DELAY, len(source)))
        self.assertEqual(expected[rk.DELAY:], source[:-rk.DELAY])
        self.assertTrue(any(expected))

        flat_state, flat_cursor = [0.0] * rk.DELAY, 0
        flat_out = [0.0] * len(source)
        rk.delay_py(source, flat_state, flat_cursor, flat_out)
        class View:
            def __init__(self, backing, lo, hi): self.backing, self.lo, self.hi = backing, lo, hi
            def __len__(self): return self.hi - self.lo
            def __getitem__(self, i): return self.backing[self.lo + i]
            def __setitem__(self, i, value): self.backing[self.lo + i] = value
        out = [0.0] * len(source)
        state2, cursor2 = [0.0] * rk.DELAY, 0
        cursor2 = rk.delay_py(source[:13], state2, cursor2, View(out, 0, 13))
        cursor2 = rk.delay_py(source[13:88], state2, cursor2, View(out, 13, 88))
        cursor2 = rk.delay_py(source[88:], state2, cursor2, View(out, 88, len(source)))
        self.assertEqual(out, flat_out)
        self.assertEqual(state2, flat_state)
        self.assertEqual(cursor2, cursor)
        reset_state, reset_cursor, reset_out = [0.0] * rk.DELAY, 0, [0.0] * len(source)
        rk.delay_py(source, reset_state, reset_cursor, reset_out)
        self.assertEqual(reset_out, flat_out)
        if rk.np is not None:
            ns, no = rk.np.zeros(rk.DELAY), rk.np.zeros(len(source))
            rk.delay_np(rk.np.asarray(source), ns, 0, no)
            self.assertEqual(no.tolist(), flat_out)

    def test_run_bounds_and_report_shape_without_timing_assertions(self):
        report = rk.run(frames=16, blocks=3, warmup=1)
        self.assertIn("cpython_reference", report["candidates"])
        self.assertTrue(report["correctness_validation_before_timing"]["passed"])
        self.assertEqual(len(report["results"]["cpython_reference"]), 3)
        with self.assertRaises(ValueError):
            rk.run(frames=513, blocks=1)


if __name__ == "__main__":
    unittest.main()
