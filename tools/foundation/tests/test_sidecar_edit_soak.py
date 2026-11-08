# SPDX-License-Identifier: GPL-3.0-only
"""No hardware: protocol-speaking subprocess stub and bounded failure vectors."""
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import sidecar_edit_soak as soak

REAL_POPEN = subprocess.Popen
STUB = r'''
import json, os, sys, time
if (len(sys.argv) != 5 or sys.argv[1:4] !=
        ['--null-audio', '--bench-summary', '--settings']):
    sys.exit(31)
mode = os.environ['SOAK_STUB_MODE']
print(json.dumps(dict(v=0, type='hello', protocol=0, session='stub-session',
                      catalogue={'modules': [{'id': [] if mode == 'bad-catalogue' else 'essentials.output'}, {'id': 'essentials.vco'}]})), flush=True)
gen = 0
for line in sys.stdin:
    command = json.loads(line)
    kind = command['type']
    if mode == 'timeout':
        time.sleep(.5)
    if mode == 'exit':
        os._exit(17)
    if mode == 'malformed':
        print('not JSON', flush=True)
        continue
    if mode == 'logs':
        sys.stderr.write('x' * 100000 + '\n')
        sys.stderr.flush()
    if mode == 'refusal':
        print(json.dumps(dict(v=0, session='stub-session', re=command['id'], ok=False,
                              code='busy', message='fake refusal', gen=str(gen))), flush=True)
        continue
    if mode == 'wrong-session':
        session = 'wrong-session'
    else:
        session = 'stub-session'
    if kind in ('loadGraph', 'addModule', 'connect', 'disconnect', 'removeModule'):
        gen += 1
    print(json.dumps(dict(v=0, session=session, re=command['id'], ok=True, gen=str(gen))), flush=True)
bench = 'bench blocks=1000 p50=1 p99=2 max=3.5 overruns=0 faults=0'
if mode == 'short-blocks':
    bench = bench.replace('blocks=1000', 'blocks=0')
elif mode == 'overruns':
    bench = bench.replace('overruns=0', 'overruns=1')
elif mode == 'faults':
    bench = bench.replace('faults=0', 'faults=2')
elif mode == 'bad-bench':
    bench = bench.replace(' p50=1 ', ' p50=garbage ')
elif mode == 'nonfinite':
    bench = bench.replace('max=3.5', 'max=1e309')
elif mode == 'bad-order':
    bench = bench.replace('p99=2', 'p99=4')
elif mode == 'u64-overflow':
    bench = bench.replace('blocks=1000', 'blocks=18446744073709551616')
elif mode == 'bad-float':
    bench = bench.replace('max=3.5', 'max=nan')
elif mode == 'extra-fields':
    bench += ' other=7'
elif mode == 'truncated':
    bench = bench.removesuffix(' faults=0')
if mode != 'missing-bench':
    print(bench, file=sys.stderr, flush=True)
if mode == 'duplicate-bench':
    print(bench, file=sys.stderr, flush=True)
'''


class EditSoakTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.stub = Path(self.temp.name) / 'engine_stub.py'
        self.stub.write_text(STUB, encoding='utf-8')

    @contextlib.contextmanager
    def engine(self, mode):
        def spawn(argv, **kwargs):
            self.assertEqual(argv[:4], [str(self.stub), '--null-audio',
                                        '--bench-summary', '--settings'])
            self.assertEqual(len(argv), 5)
            self.assertTrue(str(argv[4]).endswith('settings.json'))
            kwargs['env'] = {**os.environ, 'SOAK_STUB_MODE': mode}
            self.process = REAL_POPEN([sys.executable, str(self.stub), *argv[1:]], **kwargs)
            return self.process
        with patch.object(soak.subprocess, 'Popen', side_effect=spawn):
            yield

    def test_success_and_seed_reproducibility(self):
        with self.engine('ok'):
            a = soak.run(self.stub, .02, 278)
            b = soak.run(self.stub, .02, 278)
        self.assertEqual(a['status'], 'passed')
        self.assertEqual(a['commands'], b['commands'])
        self.assertGreaterEqual(a['cycles'], 1)
        self.assertEqual(a['steps'], 2 + 9 * a['cycles'])
        self.assertEqual(a['performance_evidence'],
                         {'blocks': 1000, 'p50': 1.0, 'p99': 2.0, 'max': 3.5,
                          'overruns': 0, 'faults': 0})

    def assert_failed(self, mode, fragment, timeout=.2):
        with self.engine(mode), self.assertRaises(soak.SoakFailure) as caught:
            soak.run(self.stub, .01, 42, timeout=timeout)
        self.assertIn('seed=42 step=', str(caught.exception))
        self.assertIn(fragment, str(caught.exception))

    def test_refusal_fails(self):
        self.assert_failed('refusal', 'refused busy')

    def test_reply_timeout_fails(self):
        self.assert_failed('timeout', 'deadline exceeded', timeout=.1)

    def test_unexpected_exit_fails(self):
        self.assert_failed('exit', 'exited 17')

    def test_malformed_stdout_fails(self):
        self.assert_failed('malformed', 'invalid/overflow stdout')

    def test_wrong_session_fails(self):
        self.assert_failed('wrong-session', 'session mismatch')

    def test_malformed_catalogue_fails_and_stops_child(self):
        self.assert_failed('bad-catalogue', 'missing required module catalogue')
        self.assertEqual(self.process.poll(), 0)

    def test_large_stderr_is_bounded_and_drained(self):
        with self.engine('logs'):
            with tempfile.TemporaryDirectory() as directory:
                client = soak.Sidecar(self.stub, Path(directory) / 'settings.json', 1)
                client.hello()
                client.request('loadGraph', swap=True, patch={'modules': []})
                self.assertEqual(client.close(), '')
                self.assertEqual(len(client.tail), 8192)

    def test_metrics_reject_missing_duplicate_and_malformed_subprocess(self):
        for mode, message in (
                ('missing-bench', 'exactly one bench line'),
                ('duplicate-bench', 'exactly one bench line'),
                ('bad-bench', 'malformed bench'),
                ('short-blocks', 'insufficient bench blocks'),
                ('overruns', 'overruns=1'),
                ('faults', 'faults=2'),
                ('nonfinite', 'bench max invalid'),
                ('bad-order', 'percentiles out of order'),
                ('u64-overflow', 'exceeds u64'),
                ('bad-float', 'malformed bench'),
                ('extra-fields', 'malformed bench'),
                ('truncated', 'malformed bench')):
            with self.subTest(mode=mode):
                self.assert_failed(mode, message)

    def test_bench_parser_boundary_conditions(self):
        good = 'bench blocks=34 p50=0 p99=1 max=1.5 overruns=0 faults=0'
        self.assertEqual(soak.parse_bench([good], .2)['blocks'], 34)
        for lines in ([], [good, good], ['not a metric'], [good + ' ']):
            with self.subTest(lines=lines), self.assertRaises(soak.SoakFailure):
                soak.parse_bench(lines, .2)
        for bad in ('blocks=-1', 'p50=-1', 'p99=NaN', 'max=inf',
                    'faults=-1', 'overruns=1.0', 'blocks=18446744073709551616'):
            line = good.replace(bad.split('=')[0] + '=' + {
                'blocks': '34', 'p50': '0', 'p99': '1', 'max': '1.5',
                'faults': '0', 'overruns': '0'}[bad.split('=')[0]], bad)
            with self.subTest(bad=bad), self.assertRaises(soak.SoakFailure):
                soak.parse_bench([line], .2)

    def test_large_logs_do_not_hide_unique_metrics(self):
        with self.engine('logs'):
            result = soak.run(self.stub, .02, 278)
        self.assertEqual(result['performance_evidence']['faults'], 0)

    def test_failure_cli_json_is_explicit_not_unavailable(self):
        with self.engine('missing-bench'):
            output = io.StringIO()
            with contextlib.redirect_stdout(output), contextlib.redirect_stderr(io.StringIO()):
                code = soak.main(['--engine', str(self.stub), '--seconds', '.02', '--seed', '278'])
        verdict = json.loads(output.getvalue())
        self.assertEqual(code, 1)
        self.assertEqual(verdict['status'], 'failed')
        self.assertNotIn('performance_evidence', verdict)
        self.assertIn('exactly one bench line', verdict['error'])

    def test_invalid_duration_emits_strict_failure_json(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(io.StringIO()):
            code = soak.main(['--engine', str(self.stub), '--seconds', 'nan'])
        self.assertEqual(code, 1)
        verdict = json.loads(output.getvalue())
        self.assertEqual(verdict['status'], 'failed')
        self.assertIsNone(verdict['seconds'])
        self.assertNotIn('NaN', output.getvalue())

    def test_invalid_limits(self):
        for seconds in (0, -1, float('nan'), float('inf'), 3601):
            with self.assertRaises(ValueError):
                soak.run(self.stub, seconds, 278)
        with self.assertRaises(ValueError):
            soak.run(self.stub, 1, -1)


if __name__ == '__main__':
    unittest.main()
