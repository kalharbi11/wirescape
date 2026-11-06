# SPDX-License-Identifier: GPL-3.0-only
"""Replay the shared protocol v0 transcript against the real null-audio sidecar."""
import json
import math
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time

LIMIT = 2_097_152
CODES = {'unknown-module', 'kind-mismatch', 'duplicate', 'not-found', 'refused',
         'invalid', 'busy', 'limit', 'unavailable'}


def strict_pairs(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError('duplicate JSON key: ' + key)
        value[key] = item
    return value


def strict_loads(raw):
    return json.loads(raw, object_pairs_hook=strict_pairs,
                      parse_constant=lambda x: (_ for _ in ()).throw(ValueError(x)))


class Audit:
    def __init__(self):
        self.errors = []

    def check(self, condition, where, reason):
        if not condition:
            self.errors.append(f'{where}: {reason}')

    def compare(self, expected, actual, where):
        # The catalogue example describes required stable IDs, not the entire installed
        # catalogue. Every expected descriptor must match; extra installed IDs are legal.
        if where.endswith('.catalogue.modules') and isinstance(actual, list):
            by_id = {item.get('id'): item for item in actual if isinstance(item, dict)}
            self.check(len(by_id) == len(actual), where, 'duplicate/malformed module IDs')
            for item in expected:
                self.check(item['id'] in by_id, where, f'missing module {item["id"]}')
                if item['id'] in by_id:
                    self.compare(item, by_id[item['id']], where + '[' + item['id'] + ']')
            return
        if expected == '*':
            valid = isinstance(actual, str) and bool(actual) and all(c.isprintable() for c in actual)
            if where.endswith(('.session', '.engine.version', '.engine.sha')):
                valid = valid and actual.isascii()
            if where.endswith('.telemetry.time'):
                valid = type(actual) in (int, float) and math.isfinite(actual) and actual >= 0
            self.check(valid, where, f'expected nonempty token/observed time, got {actual!r}')
            return
        if isinstance(expected, dict):
            self.check(isinstance(actual, dict), where, f'expected object, got {actual!r}')
            if not isinstance(actual, dict):
                return
            for key, item in expected.items():
                self.check(key in actual, where, 'missing field ' + key)
                if key in actual:
                    self.compare(item, actual[key], where + '.' + key)
            return
        if isinstance(expected, list):
            self.check(isinstance(actual, list), where, f'expected list, got {actual!r}')
            if not isinstance(actual, list):
                return
            self.check(len(actual) == len(expected), where,
                       f'list length {len(actual)} != {len(expected)}')
            for index, (item, got) in enumerate(zip(expected, actual)):
                self.compare(item, got, f'{where}[{index}]')
            return
        # Metrics and clocks are real observations, not constant sample values.
        if where.endswith(('.telemetry.tick', '.status.audio.cpuPercent',
                           '.status.audio.dropouts', '.audio.cpuPercent', '.audio.dropouts')):
            self.check(type(actual) in (int, float) and math.isfinite(actual) and actual >= 0,
                       where, f'bad measured value {actual!r}')
            return
        if where.endswith(('.status.audio.latencyMs', '.audio.latencyMs')):
            self.check(type(actual) in (int, float) and math.isfinite(actual) and
                       abs(actual - float(expected)) <= 0.001, where,
                       f'bad calculated latency {actual!r}')
            return
        if '.telemetry.meters.' in where:
            self.check(type(actual) in (int, float) and math.isfinite(actual) and actual >= 0,
                       where, f'bad observed meter {actual!r}')
            return
        self.check(type(actual) is type(expected) and actual == expected,
                   where, f'expected {expected!r}, got {actual!r}')

    def output(self):
        if self.errors:
            raise AssertionError('\n'.join(self.errors[:50]) +
                                 (f'\n... {len(self.errors) - 50} more' if len(self.errors) > 50 else ''))


def read_fixture(path):
    steps = []
    section = None
    sections = set()
    for number, raw in enumerate(Path(path).read_text(encoding='utf-8').splitlines(), 1):
        line = raw.strip()
        if len(line) == 5 and line[:3] == '# T' and line[3:].isdigit():
            section = line[2:].strip()
            sections.add(section)
        if not line or line.startswith('#'):
            continue
        assert section and line[:2] in ('> ', '< ', '@ '), f'bad fixture line {number}'
        direction, payload = line[0], line[2:]
        try:
            value = strict_loads(payload)
        except (ValueError, json.JSONDecodeError):
            assert section == 'T14' and direction == '>' and 'BROKEN' in payload, number
            value = None
        assert value is None or isinstance(value, dict), f'not an object on line {number}'
        steps.append((section, direction, value, payload, number))
    assert sections == {f'T{i:02d}' for i in range(1, 16)}, 'fixture missing cases'
    assert sum(d == '>' for _, d, *_ in steps) >= 17
    return steps


class Engine:
    def __init__(self, binary, settings, audit):
        self.audit = audit
        self.process = subprocess.Popen([binary, '--null-audio', '--settings', str(settings)],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
        self.messages = queue.Queue()
        self.reader_errors = []
        self.stderr_tail = bytearray()
        self.sequence = 0
        self.pushes = []
        self.session = None

        def stdout_reader():
            for line in self.process.stdout:
                try:
                    if not line.endswith(b'\n') or len(line.rstrip(b'\n')) > LIMIT:
                        raise ValueError('invalid protocol line framing/size')
                    value = strict_loads(line.decode('utf-8'))
                    if not isinstance(value, dict):
                        raise ValueError('non-object stdout')
                    self.messages.put(value)
                except (UnicodeError, ValueError) as exc:
                    self.reader_errors.append(f'{exc}: {line[:100]!r}')

        def stderr_reader():
            for chunk in iter(lambda: self.process.stderr.read1(4096), b''):
                self.stderr_tail.extend(chunk)
                del self.stderr_tail[:-4096]

        self.reader = threading.Thread(target=stdout_reader, daemon=True)
        self.drainer = threading.Thread(target=stderr_reader, daemon=True)
        self.reader.start()
        self.drainer.start()

    def next(self):
        try:
            value = self.messages.get(timeout=12)
        except queue.Empty as exc:
            raise AssertionError(f'engine stopped replying (exit={self.process.poll()}): '
                                 f'{bytes(self.stderr_tail).decode(errors="replace")} '
                                 f'{self.reader_errors}') from exc
        self.sequence += 1
        where = f'wire[{self.sequence}]'
        self.audit.check(value.get('v') == 0, where, 'missing protocol v0')
        if value.get('type') == 'hello':
            self.audit.check(self.session is None, where, 'unexpected second hello')
            self.audit.check('re' not in value and 'gen' not in value, where, 'hello envelope')
        else:
            self.audit.check(isinstance(value.get('gen'), str) and
                             value['gen'].isascii() and value['gen'].isdecimal() and
                             str(int(value['gen'])) == value['gen'], where, 'invalid gen string')
            self.audit.check(value.get('session') == self.session, where, 'wrong process session')
            if 're' in value:
                self.audit.check('type' not in value and type(value.get('ok')) is bool,
                                 where, 'reply envelope')
                if value.get('ok') is False:
                    self.audit.check(value.get('code') in CODES and
                                     isinstance(value.get('message'), str) and bool(value['message']),
                                     where, 'refusal code/message')
                    self.audit.check('result' not in value, where, 'result on refusal')
                else:
                    self.audit.check('code' not in value and 'message' not in value,
                                     where, 'error fields on successful reply')
            else:
                self.audit.check(value.get('type') in ('status', 'telemetry', 'devices') and
                                 'id' not in value, where, 'invalid engine push')
                self.pushes.append((self.sequence, value))
                self.check_push_shape(value, where)
        return value

    def check_push_shape(self, value, where):
        kind = value['type']
        if kind == 'status':
            status = value.get('status', {})
            audio = status.get('audio', {}) if isinstance(status, dict) else {}
            midi = status.get('midi', {}) if isinstance(status, dict) else {}
            self.audit.check(all(k in audio for k in ('state', 'deviceId', 'deviceName',
                'sampleRate', 'bufferFrames', 'latencyMs', 'cpuPercent', 'dropouts')), where,
                'status missing audio fields')
            self.audit.check(all(k in midi for k in ('devices', 'activity')), where,
                             'status missing midi fields')
        elif kind == 'telemetry':
            tele = value.get('telemetry', {})
            self.audit.check(all(k in tele for k in ('tick', 'time', 'effective', 'phase',
                'envelope', 'meters', 'scope', 'eventActivity', 'heldNotes')), where,
                'telemetry missing required fields')
            self.audit.check('outputs' not in tele, where, 'unsupported v0 telemetry outputs')
        else:
            self.audit.check(isinstance(value.get('audioDevices'), list) and
                             isinstance(value.get('midiDevices'), list), where,
                             'devices push shape')

    def hello(self):
        while True:
            message = self.next()
            if message.get('type') == 'hello':
                token = message.get('session')
                self.audit.check(isinstance(token, str) and bool(token) and token.isascii(),
                                 'hello', 'bad session token')
                self.session = token
                return message
            raise AssertionError(f'first stdout object is not hello: {message}')

    def send(self, payload):
        self.process.stdin.write(payload)
        self.process.stdin.flush()

    def reply(self, expected_re):
        while True:
            value = self.next()
            if 're' not in value:
                continue
            self.audit.check(value['re'] == expected_re, f'reply {expected_re}',
                             f'out of order/correlation: {value.get("re")!r}')
            return value

    def push(self, kind, since, generation):
        for seq, value in self.pushes:
            if seq > since and value.get('type') == kind and value.get('gen') == str(generation):
                return value
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            value = self.next()
            if 're' in value:
                self.audit.check(False, f'push {kind}', f'unexpected reply {value}')
                continue
            if value.get('type') == kind and value.get('gen') == str(generation):
                return value
        raise AssertionError(f'no {kind} push at generation {generation}')

    def saturate(self, command, generation):
        # No graph commits: deliberately slow ordered device control requests
        # let the independent stdin reader reach its 256-command pending limit.
        # --null-audio reopens only the fake paced sink, never an actual device.
        burst = [dict(v=0, id=f'golden-fill-{i}', type='selectAudioDevice',
                      deviceId='USB Stereo') for i in range(640)]
        burst.append(command)
        self.send(b''.join(json.dumps(item, separators=(',', ':')).encode() + b'\n'
                           for item in burst))
        answer = None
        busy_count = 0
        for item in burst:
            reply = self.reply(item['id'])
            self.audit.check(reply.get('gen') == str(generation), item['id'], 'control request changed gen')
            self.audit.check(reply.get('ok') is True or reply.get('code') == 'busy',
                             item['id'], f'unexpected saturation reply {reply}')
            if reply.get('code') == 'busy':
                busy_count += 1
            if item is command:
                answer = reply
        self.audit.check(busy_count > 0, 'saturation', 'no full-queue busy was observed')
        return answer

    def close(self, unfinished=b''):
        if unfinished:
            self.send(unfinished)
        self.process.stdin.close()
        try:
            code = self.process.wait(timeout=12)
            self.audit.check(code == 0, 'EOF', f'exit code {code}; {self.stderr_tail[-200:]!r}')
            self.reader.join(timeout=2)
            self.drainer.join(timeout=2)
            self.audit.check(not self.reader_errors, 'stdout', repr(self.reader_errors))
        finally:
            if self.process.poll() is None:
                self.process.kill()


def replay(binary, fixture):
    audit = Audit()
    steps = read_fixture(fixture)
    with tempfile.TemporaryDirectory(prefix='wirescape-golden-') as root:
        engine = Engine(binary, Path(root) / 'preferences.json', audit)
        generation = 0
        last_command = None
        fresh_session = None
        push_start = 0
        try:
            for section, direction, value, raw, number in steps:
                where = f'{section}:{number}'
                if direction == '@':
                    # These are Electron main-process messages, never engine stdin.
                    if value.get('type') == 'app.engineExited':
                        old_session = engine.session
                        engine.close(b'{"unfinished":')
                        engine = Engine(binary, Path(root) / 'preferences.json', audit)
                        fresh_session = old_session
                        generation = 0
                    continue
                if direction == '>':
                    last_command = value
                    if section == 'T14' and value and value.get('id') == 'r14b':
                        last_reply = engine.saturate(value, generation)
                        last_command = None
                    else:
                        engine.send((raw + '\n').encode('utf-8'))
                        last_reply = None
                    continue
                if value.get('type') == 'hello':
                    actual = engine.hello()
                    if fresh_session is not None:
                        audit.check(engine.session != fresh_session, where, 'reused process token')
                        fresh_session = None
                    audit.compare(value, actual, where)
                    continue
                if 're' in value:
                    if last_reply is None:
                        actual = engine.reply(value['re'])
                    else:
                        actual = last_reply
                    if value['gen'] == '+1':
                        generation += 1
                    elif value['gen'] != '=':
                        raise AssertionError(f'bad golden generation relation at {where}')
                    expected = dict(value, gen=str(generation))
                    audit.compare(expected, actual, where)
                    last_reply = None
                    last_command = None
                    push_start = engine.sequence
                    if section == 'T14' and value.get('code') == 'invalid':
                        # Raw parser boundaries are part of v0; preserve the fixture's
                        # malformed line, then probe additional spec-defined boundaries.
                        for raw_bytes, code in ((b'{"v":0,"v":0,"id":"dup","type":"panic"}\n', 'invalid'),
                                                (b'{"v":0,"id":"bad","type":"panic","x":"\xff"}\n', 'invalid'),
                                                (b'x' * (LIMIT + 1) + b'\n', 'limit')):
                            engine.send(raw_bytes)
                            response = engine.reply(None)
                            audit.check(response.get('code') == code and
                                        response.get('gen') == str(generation), 'raw stdin',
                                        f'expected {code} with stable gen; got {response}')
                    continue
                assert value.get('type') in ('devices', 'status', 'telemetry'), where
                actual = engine.push(value['type'], push_start, generation)
                audit.compare(dict(value, gen=str(generation)), actual, where)
            engine.close()
        finally:
            if engine.process.poll() is None:
                engine.process.kill()
    audit.output()
    print('sidecar golden v0: transcript replies, pushes, generations, raw stdin and EOF passed')


def main():
    if len(sys.argv) != 3:
        raise SystemExit('usage: golden_tests.py <wirescape-engine> <golden_v0.ndjson>')
    if sys.platform == 'win32' and os.environ.get('WIRESCAPE_SIDECAR_WINDOWS') != '1':
        print('SKIP: Windows sidecar stdout stall pending #266')
        return 77
    replay(sys.argv[1], sys.argv[2])
    return 0


if __name__ == '__main__':
    sys.exit(main())
