# SPDX-License-Identifier: GPL-3.0-only
"""No hardware: protocol-speaking subprocess stub and bounded failure vectors."""
import contextlib
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
if len(sys.argv) != 4 or sys.argv[1] != '--null-audio' or sys.argv[2] != '--settings':
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
        sys.stderr.write('x' * 100000)
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
            self.assertEqual(argv[:2], [str(self.stub), '--null-audio'])
            self.assertEqual(argv[2], '--settings')
            self.assertEqual(len(argv), 4)
            self.assertTrue(str(argv[3]).endswith('settings.json'))
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
        self.assertEqual(a['performance_evidence'], 'unavailable')

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

    def test_invalid_limits(self):
        for seconds in (0, -1, float('nan'), float('inf'), 3601):
            with self.assertRaises(ValueError):
                soak.run(self.stub, seconds, 278)
        with self.assertRaises(ValueError):
            soak.run(self.stub, 1, -1)


if __name__ == '__main__':
    unittest.main()
