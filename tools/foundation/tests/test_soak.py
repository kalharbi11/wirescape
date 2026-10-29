# SPDX-License-Identifier: GPL-3.0-only
"""CLI integration contract; durations are short and timing is never asserted."""
import json
import subprocess
import sys
import unittest

BINARY = MODULES = None


@unittest.skipUnless(BINARY, "run via abi2_soak_cli CTest")
class SoakCliTests(unittest.TestCase):
    def run_soak(self, *args, success=True):
        result = subprocess.run([BINARY, "--seconds", ".01", *args],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr)
        return json.loads(result.stdout) if success else result.stderr

    def test_each_graph_and_schema(self):
        for graph in ("mono", "poly8", "voices:16", "six_essentials_shim"):
            data = self.run_soak("--graph", graph, "--modules", MODULES)
            self.assertEqual(data["graph"], graph)
            self.assertEqual(data["blocks"], 4)
            self.assertEqual(data["allocations"], 0)
            self.assertEqual(data["faults"], 0)
            self.assertGreater(data["peak"], 0)
            stats = data["render_ns"]
            self.assertEqual(list(stats.values()), sorted(stats.values()))
            self.assert_cpu_clock(data)
            self.assertLessEqual(data["over_100_percent"], data["over_50_percent"])

    def assert_cpu_clock(self, data):
        if sys.platform == "win32":
            # Tick-quantised thread accounting must never classify a block.
            self.assertEqual(data["cpu_clock"], "unsupported")
            self.assertIsNone(data["cpu_ns"])
            self.assertIsNone(data["preempted"])
        else:
            self.assertEqual(data["cpu_clock"], "CLOCK_THREAD_CPUTIME_ID")
            cpu = data["cpu_ns"]
            self.assertEqual(list(cpu.values()), sorted(cpu.values()))
            self.assertLessEqual(data["preempted"], data["blocks"])

    def test_budget_all_voices(self):
        data = self.run_soak("--budget")
        self.assertEqual([r["graph"] for r in data["budget_runs"]],
                         [f"voices:{n}" for n in range(1, 17)])
        for row in data["budget_runs"]:
            self.assert_cpu_clock(row)
        eligible = [n for n, row in enumerate(data["budget_runs"], 1)
                    if row["render_ns"]["p99"] < row["period_ns"] * .5]
        self.assertEqual(data["largest_voices_p99_under_50_percent"], max(eligible, default=0))

    def test_invalid_arguments(self):
        for args in (("--graph", "bad"), ("--graph", "voices:17"),
                     ("--graph", "voices:1.5"), ("--graph", "voices:\n1"), ("--seconds", "nan"),
                     ("--seconds", "0"), ("--block", "0"), ("--block", "1.5"),
                     ("--rate", "48000x"), ("--seconds", "3601"),
                     ("--graph", "six_essentials_shim"), ("--unknown", "1"), ("--rate",),
                     ("--patch", "missing.json"), ("--patch", "missing.json", "--budget"),
                     ("--patch", "missing.json", "--graph", "poly8")):
            self.assertIn("wirescape_soak:", self.run_soak(*args, success=False))


if __name__ == "__main__":
    BINARY, MODULES = sys.argv[1:3]
    # unittest's decorator executes at import time; CTest supplies the executable.
    SoakCliTests.__unittest_skip__ = False
    unittest.main(argv=[sys.argv[0]])
