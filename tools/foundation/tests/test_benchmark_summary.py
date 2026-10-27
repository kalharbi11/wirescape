# SPDX-License-Identifier: GPL-3.0-only
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
import unittest


class BenchmarkSummaryTests(unittest.TestCase):
    def test_both_runner_summaries_preserve_json(self):
        workflow = (Path(__file__).resolve().parents[3] / '.github/workflows/ci.yml').read_text()
        step = workflow.split('      - name: Summarize ABI2 benchmark\n', 1)[1].split('      - name:', 1)[0]
        self.assertNotIn('        if:', step)
        script = textwrap.dedent(step.split("python - <<'PYTHON'\n", 1)[1].rsplit('          PYTHON', 1)[0])
        data = {'rows': [{'name': 'abi2_mono', 'mean_ns': 12, 'p99_ns': 14, 'worst_ns': 16}],
                'mono_ratio': .9, 'poly8_vs_eight_mono_ratio': .95, 'modulated_ratio': 2}
        for runner in ('linux-arm64', 'windows-x64'):
            with self.subTest(runner=runner), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                benchmark = root / 'build_reports/abi2/benchmark.json'
                benchmark.parent.mkdir(parents=True)
                benchmark.write_text(json.dumps(data))
                summary = root / 'summary.md'
                result = subprocess.run([sys.executable, '-c', script], cwd=root, check=True,
                                        capture_output=True, text=True,
                                        env={**os.environ, 'ABI2_RUNNER': runner, 'GITHUB_STEP_SUMMARY': str(summary)})
                self.assertEqual(json.loads(result.stdout), data)
                report = summary.read_text()
                self.assertIn(f'ABI2 benchmark ({runner})', report)
                self.assertIn('Median ns', report)
                self.assertEqual(json.loads(report.split('```json\n')[1].split('```')[0]), data)
