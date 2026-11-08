# SPDX-License-Identifier: GPL-3.0-only
"""Bounded device-free sidecar edit soak; no engine argument passthrough."""
import argparse
from collections import deque
import json
import math
from pathlib import Path
import queue
import random
import re
import subprocess
import sys
import tempfile
import threading
import time


class SoakFailure(Exception):
    pass


# Strict and anchored: neither truncated stderr tails nor unrelated diagnostics count.
FLOAT = r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?'
BENCH = re.compile(
    rf'bench blocks=(?P<blocks>[0-9]{{1,20}}) '
    rf'p50=(?P<p50>{FLOAT}) p99=(?P<p99>{FLOAT}) max=(?P<max>{FLOAT}) '
    rf'overruns=(?P<overruns>[0-9]{{1,20}}) faults=(?P<faults>[0-9]{{1,20}})')


def parse_bench(lines, seconds):
    if len(lines) != 1:
        raise SoakFailure(f'expected exactly one bench line, got {len(lines)}')
    match = BENCH.fullmatch(lines[0])
    if not match:
        raise SoakFailure('malformed bench metrics')
    values = {}
    for name in ('blocks', 'overruns', 'faults'):
        number = int(match[name])
        if number > 2**64 - 1:
            raise SoakFailure(f'bench {name} exceeds u64')
        values[name] = number
    for name in ('p50', 'p99', 'max'):
        number = float(match[name])
        if not math.isfinite(number) or number < 0:
            raise SoakFailure(f'bench {name} invalid')
        values[name] = number
    if values['p50'] > values['p99'] or values['p99'] > values['max']:
        raise SoakFailure('bench percentiles out of order')
    if values['blocks'] < 0.9 * seconds * 48000 / 256:
        raise SoakFailure('insufficient bench blocks')
    if values['overruns'] or values['faults']:
        raise SoakFailure(f'bench overruns={values["overruns"]} faults={values["faults"]}')
    return values


def unique_object(pairs):
    obj = {}
    for key, value in pairs:
        if key in obj:
            raise ValueError(f"duplicate JSON key {key}")
        obj[key] = value
    return obj


class Sidecar:
    def __init__(self, binary, settings, timeout):
        self.process = subprocess.Popen([str(binary), '--null-audio', '--bench-summary', '--settings', str(settings)],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, bufsize=0)
        self.timeout = timeout
        self.inbox = queue.Queue(maxsize=128)
        self.errors = deque(maxlen=2)
        self.tail = bytearray()
        self.bench_lines = []  # at most two candidate lines, independent of the tail
        self.step = 0
        self.gen = 0
        self.session = None
        self.counts = {}
        self.threads = [threading.Thread(target=self._stdout, daemon=True),
                        threading.Thread(target=self._stderr, daemon=True)]
        for thread in self.threads:
            thread.start()

    def _stdout(self):
        for line in iter(lambda: self.process.stdout.readline(2097154), b''):
            try:
                if len(line) > 2097153 or not line.endswith(b'\n'):
                    raise ValueError('oversized/unterminated stdout line')
                msg = json.loads(line, object_pairs_hook=unique_object,
                                 parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
                if not isinstance(msg, dict):
                    raise ValueError('non-object stdout line')
                self.inbox.put_nowait(msg)
            except (ValueError, UnicodeError, RecursionError, queue.Full) as exc:
                if not self.errors:
                    self.errors.append(f'{exc}: {line[:120]!r}')

    def _stderr(self):
        pending = bytearray()
        discarded = False
        for chunk in iter(lambda: self.process.stderr.read(4096), b''):
            self.tail.extend(chunk)
            del self.tail[:-8192]
            parts = chunk.split(b'\n')
            for part in parts[:-1]:
                if not discarded:
                    pending.extend(part)
                    if len(pending) <= 2048:
                        line = bytes(pending).removesuffix(b'\r')
                        if line.lstrip().startswith(b'bench'):
                            if len(self.bench_lines) < 2:
                                self.bench_lines.append(line.decode('ascii', errors='replace'))
                pending.clear()
                discarded = False
            pending.extend(parts[-1])
            if len(pending) > 2048:
                pending.clear()
                discarded = True

    def _next(self, deadline):
        while True:
            if self.errors:
                raise SoakFailure(f'invalid/overflow stdout: {self.errors[0]}')
            if self.process.poll() is not None and self.inbox.empty():
                raise SoakFailure(f'engine exited {self.process.returncode} before response')
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise SoakFailure('response deadline exceeded')
            try:
                return self.inbox.get(timeout=min(remaining, .05))
            except queue.Empty:
                continue

    def hello(self):
        msg = self._next(time.monotonic() + max(2.0, self.timeout))
        if (msg.get('type') != 'hello' or type(msg.get('v')) is not int or msg['v'] != 0
                or msg.get('protocol') != 0 or not isinstance(msg.get('session'), str)
                or not msg['session'] or not isinstance(msg.get('catalogue'), dict)):
            raise SoakFailure(f'invalid first hello: {str(msg)[:180]}')
        modules = msg['catalogue'].get('modules')
        if not isinstance(modules, list) or not {'essentials.output', 'essentials.vco'} <= {
                m['id'] for m in modules if isinstance(m, dict) and isinstance(m.get('id'), str)}:
            raise SoakFailure('hello missing required module catalogue')
        self.session = msg['session']

    def request(self, kind, swap=False, **fields):
        self.step += 1
        request_id = f'soak-{self.step}'
        payload = dict(v=0, id=request_id, type=kind, **fields)
        try:
            self.process.stdin.write((json.dumps(payload, separators=(',', ':')) + '\n').encode())
            self.process.stdin.flush()
        except (OSError, ValueError) as exc:
            raise SoakFailure(f'{kind}: stdin write failed: {exc}') from exc
        deadline = time.monotonic() + self.timeout
        while True:
            msg = self._next(deadline)
            if (type(msg.get('v')) is not int or msg['v'] != 0
                    or msg.get('session') != self.session):
                raise SoakFailure(f'{kind}: protocol version/session mismatch: {str(msg)[:180]}')
            generation = msg.get('gen')
            if not isinstance(generation, str) or not re.fullmatch(r'[0-9]+', generation):
                raise SoakFailure(f'{kind}: missing/malformed generation: {str(msg)[:180]}')
            if 're' not in msg:
                if msg.get('type') not in ('status', 'telemetry', 'devices'):
                    raise SoakFailure(f'{kind}: unexpected push: {str(msg)[:180]}')
                continue
            if msg.get('re') != request_id or 'type' in msg:
                raise SoakFailure(f'{kind}: wrong reply ID/type: {str(msg)[:180]}')
            if msg.get('ok') is not True:
                raise SoakFailure(f'{kind}: refused {msg.get("code")}: {msg.get("message")}')
            expected = self.gen + int(swap)
            if int(generation) != expected:
                raise SoakFailure(f'{kind}: generation {generation}, expected {expected}')
            self.gen = expected
            self.counts[kind] = self.counts.get(kind, 0) + 1
            return

    def close(self):
        problems = []
        try:
            self.process.stdin.close()  # EOF, even after a failing edit
        except OSError:
            pass
        try:
            code = self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=2)
            problems.append('engine did not exit cleanly on EOF')
        else:
            if code != 0:
                problems.append(f'engine exit status {code}')
        for thread in self.threads:
            thread.join(timeout=1)
            if thread.is_alive():
                problems.append('pipe reader did not stop')
        self.process.stdout.close()
        self.process.stderr.close()
        if self.errors:
            problems.append(f'bad stdout: {self.errors[0]}')
        return '; '.join(problems)


def run(binary, seconds, seed, timeout=5.0):
    if not math.isfinite(seconds) or not 0 < seconds <= 3600:
        raise ValueError('seconds must be finite and in (0, 3600]')
    if not 0 <= seed <= 4294967295:
        raise ValueError('seed must be in [0, 2^32-1]')
    rng = random.Random(seed)
    with tempfile.TemporaryDirectory(prefix='wirescape-soak-') as temp:
        sidecar = Sidecar(binary, Path(temp) / 'settings.json', timeout)
        error = None
        result = None
        try:
            sidecar.hello()
            sidecar.request('loadGraph', swap=True, patch={
                'format': 'wirescape.native.patch', 'version': 2,
                'modules': [{'id': 7, 'module': 'essentials.output'}], 'connections': []})
            end = time.monotonic() + seconds
            cycles, next_id = 0, 8
            while cycles == 0 or time.monotonic() < end:
                module = next_id
                next_id += 1  # never reuse a module ID within this document
                edge = {'kind': 'signal', 'from': {'module': module, 'port': 'audio'},
                        'to': {'module': 7, 'port': 'audio'}}
                sidecar.request('addModule', swap=True, moduleId=module, moduleType='essentials.vco')
                sidecar.request('setParam', moduleId=module, paramId='frequency',
                                value=rng.choice((110, 220, 330, 440, 660, 880)))
                sidecar.request('connect', swap=True, connection=edge)
                sidecar.request('setParam', moduleId=module, paramId='frequency',
                                value=rng.choice((110, 220, 330, 440, 660, 880)))
                sidecar.request('disconnect', swap=True, connection=edge)
                sidecar.request('removeModule', swap=True, moduleId=module)
                sidecar.request('noteOn', note=60, velocity=.8)
                sidecar.request('noteOff', note=60)
                sidecar.request('panic')
                cycles += 1
                if time.monotonic() < end:
                    time.sleep(min(.1, end - time.monotonic()))
            sidecar.request('panic')
            result = {'status': 'passed', 'seed': seed, 'seconds': seconds,
                      'cycles': cycles, 'steps': sidecar.step, 'session': sidecar.session,
                      'generation': str(sidecar.gen), 'commands': sidecar.counts,
                      'audio': 'null-sink', 'midi': 'fake-only'}
        except (SoakFailure, OSError, ValueError) as exc:
            error = str(exc)
        shutdown = sidecar.close()
        if not error and not shutdown:
            try:
                result['performance_evidence'] = parse_bench(sidecar.bench_lines, seconds)
            except SoakFailure as exc:
                error = str(exc)
        if error or shutdown:
            tail = bytes(sidecar.tail).decode('utf-8', errors='replace')[-1000:]
            raise SoakFailure(f'seed={seed} step={sidecar.step}: {error or ""}; '
                              f'shutdown={shutdown or "clean"}; stderr_tail={tail!r}')
        return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--engine', type=Path, help='explicit local wirescape-engine executable')
    parser.add_argument('--seconds', type=float, required=True, help='duration, up to 3600 s')
    parser.add_argument('--seed', type=int, default=278)
    args = parser.parse_args(argv)  # no engine args/confirm flags are accepted
    try:
        native = Path(__file__).resolve().parents[1] / 'build' / 'native'
        choices = [native / 'wirescape-engine', native / 'wirescape-engine.exe',
                   native / 'Release' / 'wirescape-engine.exe']
        binary = args.engine if args.engine is not None else next((p for p in choices if p.is_file()), None)
        if binary is None or not binary.is_file():
            raise ValueError('selected engine is not a local file (build first or use --engine)')
        print(json.dumps(run(binary.resolve(), args.seconds, args.seed), sort_keys=True))
        return 0
    except (OSError, ValueError, SoakFailure) as exc:
        # Machine-readable failure is never an unavailable or zero-valued success.
        duration = args.seconds if math.isfinite(args.seconds) else None
        print(json.dumps({'status': 'failed', 'seed': args.seed, 'seconds': duration,
                          'error': str(exc)}, sort_keys=True, allow_nan=False))
        print(f'sidecar edit soak: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
