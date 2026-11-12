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
bench = ('bench blocks=1000 p50=1 p99=2 max=3.5 overruns=0 faults=0 '
         'overrun_render_count=0 overrun_render_max_us=0 '
         'overrun_wake_count=0 overrun_wake_max_us=0')
if mode == 'short-blocks':
    bench = bench.replace('blocks=1000', 'blocks=0')
elif mode == 'overruns':
    bench = bench.replace('overruns=0', 'overruns=1')
elif mode == 'faults':
    bench = bench.replace('faults=0', 'faults=2')
elif mode in ('wake-only', 'render-overrun'):
    bench = bench.replace('overruns=0', 'overruns=1')
    if mode == 'wake-only':
        bench = bench.replace('overrun_wake_count=0', 'overrun_wake_count=1')
        bench = bench.replace('overrun_wake_max_us=0', 'overrun_wake_max_us=5795.14')
    else:
        bench = bench.replace('overrun_render_count=0', 'overrun_render_count=1')
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
    bench = bench.rsplit(' ', 1)[0]
if mode != 'missing-bench':
    print(bench, file=sys.stderr, flush=True)
if mode == 'duplicate-bench':
    print(bench, file=sys.stderr, flush=True)
elif mode == 'unterminated-second-bench':
    sys.stderr.write(bench)
    sys.stderr.flush()
elif mode == 'oversized-second-bench':
    sys.stderr.write('bench ' + 'x' * 2100 + '\n')
    sys.stderr.flush()
'''


class EditSoakTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.stub = Path(self.temp.name) / 'engine_stub.py'
        self.stub.write_text(STUB, encoding='utf-8')

    @contextlib.contextmanager
    def engine(self, mode):
        modes = iter(mode) if isinstance(mode, tuple) else None
        self.launches = 0
        def spawn(argv, **kwargs):
            self.launches += 1
            # Windows can spell the same temp file with 8.3 or long names.
            self.assertTrue(os.path.samefile(argv[0], self.stub))
            self.assertEqual(argv[1:4], ['--null-audio', '--bench-summary', '--settings'])
            self.assertEqual(len(argv), 5)
            self.assertTrue(str(argv[4]).endswith('settings.json'))
            kwargs['env'] = {**os.environ, 'SOAK_STUB_MODE': next(modes) if modes else mode}
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
                          'overruns': 0, 'faults': 0, 'overrun_render_count': 0,
                          'overrun_render_max_us': 0.0, 'overrun_wake_count': 0,
                          'overrun_wake_max_us': 0.0})

    def test_overrun_diagnostics_preserved_in_failure(self):
        line = ('bench blocks=1000 p50=1 p99=2 max=3.5 overruns=1 faults=0 '
                'overrun_render_count=0 overrun_render_max_us=42 '
                'overrun_wake_count=1 overrun_wake_max_us=6000')
        with self.assertRaises(soak.SoakFailure) as caught:
            soak.parse_bench([line], 2)
        self.assertIn('overruns=1', str(caught.exception))
        self.assertIn('overrun_render_max_us=42.0', str(caught.exception))
        self.assertIn('overrun_wake_max_us=6000.0', str(caught.exception))
        for field, invalid in (('overrun_render_count=0', 'overrun_render_count=2'),
                               ('overrun_wake_count=1', 'overrun_wake_count=18446744073709551616'),
                               ('overrun_render_max_us=42', 'overrun_render_max_us=-1'),
                               ('overrun_wake_max_us=6000', 'overrun_wake_max_us=1e309')):
            with self.subTest(field=field), self.assertRaises(soak.SoakFailure):
                soak.parse_bench([line.replace(field, invalid)], 2)

    def assert_failed(self, mode, fragment, timeout=.2):
        with self.engine(mode), self.assertRaises(soak.SoakFailure) as caught:
            soak.run(self.stub, .01, 42, timeout=timeout)
        self.assertIn('seed=42 step=', str(caught.exception))
        self.assertIn(fragment, str(caught.exception))

    def test_hosted_windows_wake_retry_is_one_shot_and_strict(self):
        cases = [(('wake-only', 'ok'), True, 0, 2),
                 (('wake-only', 'wake-only'), True, 1, 2),
                 (('wake-only', 'render-overrun'), True, 1, 2),
                 (('render-overrun', 'ok'), True, 1, 1),
                 (('faults', 'ok'), True, 1, 1),
                 (('wake-only', 'ok'), False, 1, 1)]
        for modes, hosted, expected, launches in cases:
            with self.subTest(modes=modes, hosted=hosted), self.engine(modes):
                stdout, stderr = io.StringIO(), io.StringIO()
                with patch.object(soak, 'hosted_windows', return_value=hosted):
                    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                        status = soak.main(['--engine', str(self.stub), '--seconds', '.02'])
                self.assertEqual(status, expected)
                self.assertEqual(self.launches, launches)
                verdict = json.loads(stdout.getvalue())
                self.assertEqual(verdict['status'], 'passed' if not status else 'failed')
                if hosted and modes == ('wake-only', 'ok'):
                    retry = verdict.get('wake_retry')
                    self.assertIsInstance(retry, dict)
                    self.assertIn('overrun_wake_count=1', retry['attempt1'])
                    self.assertEqual(retry['attempt2'], 'passed')
                else:
                    self.assertNotIn('wake_retry', verdict)
                if hosted and modes[0] == 'wake-only':
                    self.assertIn('wake-only attempt 1 failed', stderr.getvalue())
                    self.assertEqual('attempt 2 passed' in stderr.getvalue(), status == 0)

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
                ('unterminated-second-bench', 'exactly one bench line'),
                ('oversized-second-bench', 'exactly one bench line'),
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
        good = ('bench blocks=34 p50=0 p99=1 max=1.5 overruns=0 faults=0 '
                'overrun_render_count=0 overrun_render_max_us=0 '
                'overrun_wake_count=0 overrun_wake_max_us=0')
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

    def test_incomplete_bench_candidate_fails_even_when_it_is_the_only_record(self):
        from types import SimpleNamespace
        good = (b'bench blocks=1000 p50=1 p99=2 max=3.5 overruns=0 faults=0 '
                b'overrun_render_count=0 overrun_render_max_us=0 '
                b'overrun_wake_count=0 overrun_wake_max_us=0')
        for raw in (good, b'bench ' + b'x' * 2100 + b'\n'):
            with self.subTest(bytes=len(raw)):
                client = soak.Sidecar.__new__(soak.Sidecar)
                client.process = SimpleNamespace(stderr=io.BytesIO(raw))
                client.tail = bytearray()
                client.bench_lines = []
                client._stderr()
                self.assertEqual(len(client.bench_lines), 1)
                with self.assertRaises(soak.SoakFailure):
                    soak.parse_bench(client.bench_lines, .02)

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
