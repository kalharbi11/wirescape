# SPDX-License-Identifier: GPL-3.0-only
"""Process conformance: pipes, null sink and an explicitly fake device catalogue."""
import atexit
import json
import os
import queue
import re
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

BINARY, EXAMPLES = sys.argv[1:3]


def strict_object(pairs):
    result = {}
    for key, value in pairs:
        assert key not in result, f'duplicate output key {key}'
        result[key] = value
    return result


class Engine:
    def __init__(self, settings, *extra):
        self.process = subprocess.Popen([BINARY, '--null-audio', '--settings', str(settings), *extra],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
        self.messages = queue.Queue()
        self.errors = []
        self.serial = 0
        self.last_gen = '0'
        self.pushes = []

        def read():
            # Record a bad line and keep draining: a dead reader would block the engine's
            # stdout and hide the real problem behind its write watchdog.
            for line in self.process.stdout:
                try:
                    assert len(line.rstrip(b'\n')) <= 2097152
                    assert line.endswith(b'\n')
                    self.messages.put(json.loads(line, object_pairs_hook=strict_object))
                except Exception as error:
                    self.errors.append(f'{type(error).__name__}: {error}; line: {line[:300]!r}')
        # Drain stderr continuously: an undrained pipe (about 4 KiB on Windows) blocks the
        # engine's diagnostics and with them the whole process.
        self.stderr_tail = bytearray()

        def drain():
            for chunk in iter(lambda: self.process.stderr.read1(4096), b''):
                self.stderr_tail.extend(chunk)
                del self.stderr_tail[:-8192]
        atexit.register(lambda: self.process.kill() if self.process.poll() is None else None)
        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()
        self.drainer = threading.Thread(target=drain, daemon=True)
        self.drainer.start()
        try:
            self.hello = self.messages.get(timeout=10)
        except queue.Empty as error:
            exit_code = self.process.poll()
            if exit_code is None:
                raise AssertionError(f'engine is still running but emitted no hello within 10s; stderr: {self.stderr()}') from error
            stderr = self.stderr()
            raise AssertionError(f'engine exited {exit_code} without hello; stderr: {stderr}') from error
        assert self.hello['type'] == 'hello' and self.hello['protocol'] == 0
        self.session = self.hello['session']
        assert self.session and self.hello['v'] == 0
        assert 'setOsc' not in self.hello['capabilities']
        ids = {module['id'] for module in self.hello['catalogue']['modules']}
        assert {'essentials.output', 'essentials.vco', 'midi.in', 'midi.cc_to_cv'} <= ids
        for module in self.hello['catalogue']['modules']:
            assert {'id', 'name', 'category', 'ports', 'parameters'} <= module.keys()
            for port in module['ports']:
                assert {'id', 'name', 'direction', 'kind', 'hint', 'poly', 'stereo_pair'} <= port.keys()

    def stderr(self):
        self.drainer.join(timeout=1) if self.process.poll() is not None else None
        tail = bytes(self.stderr_tail).decode(errors='replace')[-4096:]
        return f'{tail}; reader errors: {self.errors[:3]}'

    def receive(self):
        try:
            message = self.messages.get(timeout=10)
        except queue.Empty as error:
            exit_code = self.process.poll()
            if exit_code is None:
                raise AssertionError(f'engine is still running but emitted no reply within 10s; stderr: {self.stderr()}') from error
            stderr = self.stderr()
            raise AssertionError(f'engine exited {exit_code} before reply; stderr: {stderr}') from error
        assert message['session'] == self.session and message['v'] == 0
        if message.get('type') != 'hello':
            assert isinstance(message['gen'], str) and message['gen'].isdigit()
        return message

    def reply(self, request_id):
        while True:
            message = self.receive()
            if 're' in message:
                assert 'type' not in message
                assert message['re'] == request_id, message
                self.last_gen = message['gen']
                return message
            assert 're' not in message
            self.pushes.append(message)

    def raw(self, data, request_id=None):
        self.process.stdin.write(data)
        self.process.stdin.flush()
        return self.reply(request_id)

    def call(self, method, expected=True, **payload):
        self.serial += 1
        request_id = f'r{self.serial}'
        command = dict(v=0, id=request_id, type=method, **payload)
        result = self.raw(json.dumps(command).encode() + b'\n', request_id)
        assert result['ok'] == expected, result
        if not expected:
            assert result['code'] and result['message']
        return result

    def close(self, unfinished=b''):
        self.process.stdin.write(unfinished)
        self.process.stdin.close()
        try:
            assert self.process.wait(timeout=10) == 0, self.stderr()
        finally:
            if self.process.poll() is None:
                self.process.kill()
            self.reader.join(timeout=5)
        assert not self.errors, self.errors



def telemetry_when(engine, gen, predicate, timeout=8.0):
    """Read a same-generation complete snapshot satisfying a render-backed condition."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        message = engine.pushes.pop(0) if engine.pushes else engine.receive()
        if message.get('type') == 'telemetry' and message['gen'] == gen:
            data = message['telemetry']
            assert all(isinstance(data[key], dict)
                       for key in ('effective', 'phase', 'envelope', 'eventActivity', 'effective_pitch'))
            if predicate(data):
                return data
    raise AssertionError(f'no matching telemetry at gen {gen}: {engine.stderr()}')


def learn_status_when(engine, predicate, timeout=4):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        message = engine.pushes.pop(0) if engine.pushes else engine.receive()
        if message.get('type') == 'status' and predicate(message['status']['midi']):
            return message['status']['midi']
    raise AssertionError('No matching MIDI Learn status')


def minimal():
    return dict(format='wirescape.native.patch', version=2,
                modules=[dict(id=7, module='essentials.output')], connections=[],
                ui=dict(labels={'7': 'Out'}, opaque={'unchanged': True}))


def edge(kind, source, output, dest, port, **extra):
    return dict(kind=kind, **{'from': dict(module=source, port=output),
                            'to': dict(module=dest, port=port)}, **extra)


with tempfile.TemporaryDirectory() as directory:
    settings = Path(directory) / 'settings.json'
    engine = Engine(settings, '--confirm-audio', '--confirm-midi', '--confirm-lan')  # null overrides audio/MIDI; no OSC routes
    assert 'midiLearn' in engine.hello['capabilities']
    assert 'learn' not in engine.hello['midi']
    assert engine.call('midiLearn', armed=True)['result'] == {'armed': True}
    assert learn_status_when(engine, lambda m: m.get('learn') == {'armed': True})['learn'] == {'armed': True}
    assert engine.call('midiLearn', False, armed='yes')['code'] == 'invalid'
    assert engine.call('midiLearn', armed=False)['result'] == {'armed': False}
    assert 'learn' not in learn_status_when(engine, lambda m: 'learn' not in m)
    assert engine.call('midiLearn', armed=True)['result'] == {'armed': True}
    assert engine.call('midiLearn', armed=True)['result'] == {'armed': True}  # rearm resets timeout/observation
    assert engine.call('midiLearn', armed=False)['result'] == {'armed': False}
    patch = minimal()
    original = json.dumps(patch)
    assert engine.call('loadGraph', patch=patch)['gen'] == '1'
    expected_commits = [('d5', '1')]
    assert json.dumps(patch) == original
    refused = engine.call('addModule', False, moduleId=8, moduleType='absent.vco')
    assert refused['code'] == 'unknown-module' and refused['gen'] == '1'
    assert engine.call('addModule', moduleId=9, moduleType='essentials.vco', params={'frequency': 220})['gen'] == '2'
    cable = edge('signal', 9, 'audio', 7, 'audio')
    assert engine.call('connect', connection=cable)['gen'] == '3'
    assert engine.call('connect', False, connection={**cable, 'opaque': 'ignored'})['code'] == 'duplicate'
    engine.call('disconnect', connection={**cable, 'amount': 0.9, 'opaque': 'ignored'})
    engine.call('connect', connection=cable)
    invalid = engine.call('connect', False, connection=edge('event', 9, 'audio', 7, 'audio'))
    assert invalid['code'] == 'kind-mismatch' and invalid['gen'] == '5'
    assert engine.call('setParam', moduleId=9, paramId='frequency', value=440)['gen'] == '5'
    assert engine.call('setParam', False, moduleId=9, paramId='absent', value=440)['code'] == 'not-found'
    engine.call('noteOn', note=60, velocity=0.8)
    engine.call('noteOff', note=60)
    assert engine.call('panic')['gen'] == '5'
    engine.call('addModule', moduleId=10, moduleType='essentials.lfo')
    modulation = edge('modulation', 10, 'control', 9, 'frequency', amount=0.2)
    engine.call('connect', connection=modulation)
    assert engine.call('setModAmount', connection={**modulation, 'amount': 'ignored'}, amount=-0.5)['gen'] == '8'
    engine.call('addModule', moduleId=12, moduleType='midi.cc_to_cv')
    engine.call('ccMap.set', moduleId=12, row=0, mapping=dict(channel=0, controller=21, fourteen_bit=False, bipolar=False, smoothing_ms=10))
    assert engine.call('ccMap.set', False, moduleId=12, row=8, mapping={})['code'] == 'limit'
    assert engine.call('ccMap.set', False, moduleId=7, row=0, mapping={})['code'] == 'not-found'
    engine.call('removeModule', moduleId=10)
    assert engine.call('addModule', False, moduleId=10, moduleType='essentials.lfo')['code'] == 'duplicate'
    generation = engine.last_gen
    # Every refusal preserves the committed graph and generation.
    for changed in [dict(outputs=[]), dict(outputs=[dict(module=999, port='audio', bus_channel=0)]),
                    dict(modules=[dict(id=7, module='absent.module')]),
                    dict(modules=[dict(id=7, module='essentials.output'), dict(id=7, module='essentials.vco')]),
                    dict(modules=[dict(id=i+1, module='essentials.output') for i in range(65)])]:
        assert engine.call('loadGraph', False, patch={**minimal(), **changed})['gen'] == generation
    assert engine.call('setOsc', False, cfg={'enabled': True})['code'] == 'unavailable'
    audio = engine.call('listAudioDevices')['result']['devices']
    assert audio[0]['id'] == audio[0]['name'] == 'USB Stereo'
    engine.call('selectAudioDevice', deviceId=audio[0]['id'])
    midi = engine.call('listMidiDevices')['result']['devices']
    assert midi[0]['id'] == midi[0]['name'] + '|' + midi[0]['system_name']
    engine.call('setMidiEnabled', deviceId=midi[0]['id'], on=True)
    engine.call('setMidiEnabled', deviceId=midi[0]['id'], on=False)
    # Windows on→off→on regression: teardown must always return a terminal reply.
    engine.call('setMidiEnabled', deviceId=midi[0]['id'], on=True)
    engine.call('setMidiEnabled', deviceId=midi[0]['id'], on=False)
    assert engine.call('listMidiDevices')['result']['devices'][0]['enabled'] is False
    assert json.loads(settings.read_text())['audioDeviceId'] == 'USB Stereo'
    for data in [b'{"v":0,"id":"bad",BROKEN\n', b'{"v":0,"v":0,"id":"duplicate","type":"panic"}\n',
                 b'{"v":0,"id":"nested","type":"loadGraph","patch":{"a":1,"a":2}}\n',
                 b'[]\n', b'\xef\xbb\xbf{}\n', b'{"a":"\xff"}\n', b'{"a":NaN}\n',
                 b'{"a":' + b'['*65 + b'0' + b']'*65 + b'}\n']:
        result = engine.raw(data)
        assert result['code'] == 'invalid' and result['gen'] == generation
    result = engine.raw(b'x' * 2097153 + b'\n')
    assert result['code'] == 'limit'
    assert engine.call('panic')['gen'] == generation
    # 1000 parameter commands: ordering, bounded backpressure, process survives.
    pending = []
    for index in range(1000):
        request_id = f'rapid{index}'
        pending.append(request_id)
        engine.process.stdin.write(json.dumps(dict(v=0, id=request_id, type='setParam', moduleId=9,
                                                   paramId='frequency', value=220 + index % 100)).encode() + b'\n')
        if index % 50 == 49:
            engine.process.stdin.flush()
            for request_id in pending:
                response = engine.reply(request_id)
                assert response['gen'] == generation
                assert response['ok'] or response['code'] == 'busy'
            pending.clear()
    # A rejected graph still renders the original oscillator; only observed meters.
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        message = engine.receive()
        if message.get('type') == 'telemetry':
            telemetry = message['telemetry']
            assert all(isinstance(telemetry[key], dict)
                       for key in ['effective', 'phase', 'envelope', 'scope', 'eventActivity'])
            assert telemetry['phase'] == {} and telemetry['scope'] == {}
            if telemetry['meters'].get('7', [0])[0] > 0:
                break
    else:
        raise AssertionError('no measured null-sink output')
    # Owner v1: stereo patches route one Output per side; both meters must be observed.
    stereo = dict(format='wirescape.native.patch', version=2,
                  modules=[dict(id=1, module='essentials.vco', parameters=dict(frequency=220)),
                           dict(id=2, module='essentials.vco', parameters=dict(frequency=330)),
                           dict(id=8, module='essentials.output'), dict(id=9, module='essentials.output')],
                  connections=[edge('signal', 1, 'audio', 8, 'audio'), edge('signal', 2, 'audio', 9, 'audio')],
                  outputs=[dict(module=8, port='audio', bus_channel=0),
                           dict(module=9, port='audio', bus_channel=1)])
    engine.call('loadGraph', patch=stereo)
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        message = engine.receive()
        if message.get('type') == 'telemetry':
            meters = message['telemetry']['meters']
            if all(meters.get(key, [0, 0])[0] > 0 and meters[key][1] > 0 for key in ('8', '9')):
                break
    else:
        raise AssertionError('stereo Output meters not observed')
    for path in sorted(Path(EXAMPLES).glob('class_demo_*.json')):
        engine.call('loadGraph', patch=json.loads(path.read_text()))
        engine.call('noteOn', note=60, velocity=0.8)
    # Successful loadGraph resets the incremental ID namespace.
    loaded = engine.call('loadGraph', patch=minimal())
    expected_commits.append(('d5', loaded['gen']))
    engine.call('addModule', moduleId=10, moduleType='essentials.lfo')
    # Exercise the D7 sidecar seam with the fake MIDI worker running. No device opens.
    fake_midi = engine.call('listMidiDevices')['result']['devices'][0]
    engine.call('setMidiEnabled', deviceId=fake_midi['id'], on=True)
    old_session = engine.session
    live_edge = edge('event', 20, 'midi', 21, 'midi')
    for method, payload in [
        ('addModule', dict(moduleId=20, moduleType='midi.in')),
        ('addModule', dict(moduleId=21, moduleType='midi.to_cv')),
        ('connect', dict(connection=live_edge)),
        ('disconnect', dict(connection=live_edge)),
        ('connect', dict(connection=live_edge)),
        ('removeModule', dict(moduleId=20)),
        ('removeModule', dict(moduleId=21)),
    ]:
        prior = int(engine.last_gen)
        result = engine.call(method, **payload)
        assert int(result['gen']) == prior + 1
        expected_commits.append(('live', result['gen']))
        assert engine.session == old_session and engine.process.poll() is None
    # A refused live edit must not advance gen or tear down the running session.
    prior = engine.last_gen
    assert engine.call('addModule', False, moduleId=20, moduleType='midi.in')['gen'] == prior
    assert 'stop' not in engine.stderr().lower(), engine.stderr()
    engine.call('setMidiEnabled', deviceId=fake_midi['id'], on=False)
    engine.close(b'{"unfinished":')
    # After EOF the stderr drainer has joined: assert one path per accepted gen.
    # This fails if an incremental edit silently takes D5, or a refusal logs a commit.
    commits = [line for line in engine.stderr().splitlines() if line.startswith('commit path=')]
    for path, gen in expected_commits:
        assert [line for line in commits if line.endswith(f' gen={gen}')] == [
            f'commit path={path} gen={gen}'], engine.stderr()
    # A2: true rendered taps, complete snapshots, and first-burst cursor reset.
    observed = Engine(Path(directory) / 'telemetry.json')
    tap_patch = minimal()
    tap_patch['modules'] += [
        dict(id=9, module='essentials.vco', parameters=dict(frequency=440)),
        dict(id=10, module='essentials.lfo'),
        dict(id=12, module='host.keyboard'),
        dict(id=14, module='essentials.adsr'),
    ]
    mod = edge('modulation', 10, 'control', 9, 'frequency', amount=0.25)
    tap_patch['connections'] = [edge('signal', 9, 'audio', 7, 'audio'), mod,
                                edge('signal', 12, 'gate', 14, 'gate'),
                                edge('signal', 14, 'envelope', 9, 'fm')]
    first = observed.call('loadGraph', patch=tap_patch)['gen']
    telemetry_when(observed, first, lambda t:
                   'frequency' in t['effective'].get('9', {}) and
                   abs(t['effective']['9']['frequency'] - 440) > 0.01)
    # B-wire: real channel-0 LFO phase must advance in the bounded range.
    first_phase = telemetry_when(observed, first, lambda t:
        isinstance(t['phase'].get('10'), (int, float)) and
        0 <= t['phase']['10'] < 1)['phase']['10']
    telemetry_when(observed, first, lambda t:
        isinstance(t['phase'].get('10'), (int, float)) and
        0 <= t['phase']['10'] < 1 and
        abs(t['phase']['10'] - first_phase) > 0.005)
    observed.call('noteOn', note=60, velocity=0.8)
    telemetry_when(observed, first, lambda t: t['eventActivity'].get('12:notes') == 1)
    telemetry_when(observed, first, lambda t: t['envelope'].get('14', 0) > 0)
    telemetry_when(observed, first, lambda t:
                   0.05 <= t['eventActivity'].get('12:notes', 0) < 1)
    # Engine2 restarts counts at zero on replan; the next one-event burst may
    # equal the old count, so stale cursors would silently suppress the pulse.
    edited = observed.call('addModule', moduleId=13, moduleType='essentials.vco')['gen']
    observed.call('noteOn', note=61, velocity=0.8)
    telemetry_when(observed, edited, lambda t: t['eventActivity'].get('12:notes') == 1)
    # Full D5 graph replacement must reset the same cursors and pulse decay too.
    replaced = observed.call('loadGraph', patch=tap_patch)['gen']
    observed.call('noteOn', note=62, velocity=0.8)
    telemetry_when(observed, replaced, lambda t: t['eventActivity'].get('12:notes') == 1)
    telemetry_when(observed, replaced, lambda t: t['envelope'].get('14', 0) > 0)
    observed.call('noteOff', note=62)
    telemetry_when(observed, replaced, lambda t: t['envelope'].get('14', 1) <= 0.01)
    disconnected = observed.call('disconnect', connection=mod)['gen']
    telemetry_when(observed, disconnected, lambda t: t['effective'] == {})
    # Replacing the graph must remove phase immediately, even with reused IDs.
    no_lfo = observed.call('loadGraph', patch=minimal())['gen']
    telemetry_when(observed, no_lfo, lambda t: t['phase'] == {})
    # OSC-3 nominal pitch on the same render-backed map, including unpatched
    # oscillators and unpitched Plaits models (nominal ≠ audible fundamental).
    pitch_patch = minimal()
    pitch_patch['modules'] += [
        dict(id=9, module='essentials.vco', parameters=dict(frequency=440)),
        dict(id=8, module='ports.macro_oscillator', parameters=dict(coarse=9)),
    ]
    pitch_gen = observed.call('loadGraph', patch=pitch_patch)['gen']
    observed_pitch = telemetry_when(observed, pitch_gen, lambda t:
        '9' in t['effective_pitch'] and '8' in t['effective_pitch'])
    assert abs(observed_pitch['effective_pitch']['9'] - 69) < 0.02
    assert abs(observed_pitch['effective_pitch']['8'] - 69) < 0.02
    # Successful graph replacements must evict the previous instance values.
    blank_gen = observed.call('loadGraph', patch=minimal())['gen']
    telemetry_when(observed, blank_gen, lambda t: t['effective_pitch'] == {})
    # Same-generation Groove tuples and typed tracks are one uncabled render snapshot.
    assert 'telemetry.playhead' in observed.hello['capabilities']
    groove_patch = minimal()
    groove_patch['modules'].append(dict(id=1, module='sequencing.groove',
        config=dict(version=1, bpm=300, tracks=[dict(steps=[dict(kind='note', key=60, velocity=1)])])))
    groove_gen = observed.call('loadGraph', patch=groove_patch)['gen']
    stopped = telemetry_when(observed, groove_gen, lambda t: t['playhead'].get('1') == [-1, -1])
    assert stopped['groove']['1'][0] == dict(step=-1, loop=1, playing=False, fired=False)
    observed.call('setParam', moduleId=1, paramId='run', value=1)
    playing = telemetry_when(observed, groove_gen, lambda t: t['groove'].get('1', [{}])[0].get('playing'))
    for track, position in zip(playing['groove']['1'], playing['playhead']['1']):
        assert track['step'] == position and track['playing'] == (position >= 0)
        assert isinstance(track['loop'], int) and isinstance(track['fired'], bool)
    observed.call('setParam', moduleId=1, paramId='stop', value=1)
    telemetry_when(observed, groove_gen, lambda t: t['playhead'].get('1') == [-1, -1])
    # Reused node IDs after a replacement cannot retain the old Groove entry.
    cleared_gen = observed.call('loadGraph', patch=minimal())['gen']
    telemetry_when(observed, cleared_gen, lambda t: t['playhead'] == {} and t['groove'] == {})
    observed.close()
    restarted = Engine(settings)
    assert restarted.session != old_session
    assert restarted.call('loadGraph', patch=minimal())['gen'] == '1'
    restarted.close()
    # sidecar_bench_summary: opt-in stderr only, once after clean EOF teardown.
    bench = Engine(Path(directory) / 'bench.json', '--bench-summary')
    bench.call('loadGraph', patch=minimal())
    time.sleep(1.0)
    bench.close()
    bench.drainer.join(timeout=5)
    lines = [line for line in bytes(bench.stderr_tail).decode(errors='replace').splitlines()
             if line.startswith('bench ')]
    assert len(lines) == 1, lines
    match = re.fullmatch(
        r'bench blocks=(\d+) p50=(\d+) p99=(\d+) max=(\d+(?:\.\d+)?(?:e[+-]?\d+)?) overruns=(\d+) faults=(\d+) '
        r'overrun_render_count=(\d+) overrun_render_max_us=([0-9.e+-]+) '
        r'overrun_wake_count=(\d+) overrun_wake_max_us=([0-9.e+-]+)',
        lines[0])
    assert match, lines[0]
    blocks, p50, p99, maximum, overruns, faults = map(float, match.groups()[:6])
    assert blocks > 0 and 0 <= p50 <= p99 <= maximum and overruns >= 0 and faults == 0
    render_count, render_max_us, wake_count, wake_max_us = map(float, match.groups()[6:])
    assert 0 <= render_count <= overruns and 0 <= wake_count <= overruns
    assert 0 <= render_max_us < float('inf') and 0 <= wake_max_us < float('inf')
    # Long block periods cannot delay hello or EOF teardown. 8 kHz x 4096 frames is the
    # longest supported period (~512 ms); the engine refuses rates below 8 kHz.
    started = time.monotonic()
    slow = Engine(Path(directory) / 'slow.json', '--rate', '8000', '--block', '4096')
    assert time.monotonic() - started < 2.0, 'hello waited on block-paced work'
    slow.call('loadGraph', patch=minimal())
    closing = time.monotonic()
    slow.close()
    assert time.monotonic() - closing < 2.0, 'EOF teardown waited on block-paced work'
    for flags in [('--unknown',), ('--rate', '0'), ('--block', '4097'), ('--device',), ('--bench-summary',)]:
        result = subprocess.run([BINARY, *flags], input=b'', capture_output=True, timeout=10)
        assert result.returncode != 0 and not result.stdout
# OSC B: no physical devices and no LAN bind. An unconfirmed per-route
# lan:true still accepts the patch and listens only on 127.0.0.1.
with tempfile.TemporaryDirectory() as osc_dir:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as occupied:
        occupied.bind(('127.0.0.1', 0))
        port = occupied.getsockname()[1]
        osc_engine = Engine(Path(osc_dir) / 'settings.json')
        assert osc_engine.hello['capabilities'] == ['midiLearn', 'telemetry.playhead',
                                                    'setSampleRate', 'setBufferFrames', 'osc']
        osc_patch = minimal()
        osc_patch['modules'].append(dict(id=9, module='osc.in',
                                         config=dict(port=port, lan=True,
                                                     rows=[dict(pattern='/pico1/knob/1', kind='cv',
                                                                smoothing_ms=0)])))
        gen = osc_engine.call('loadGraph', patch=osc_patch)['gen']

        def osc_route_when(predicate):
            until = time.monotonic() + 8
            while time.monotonic() < until:
                message = osc_engine.pushes.pop(0) if osc_engine.pushes else osc_engine.receive()
                if message.get('type') != 'status' or message.get('gen') != osc_engine.last_gen:
                    continue
                osc = message['status']['osc']
                assert {'enabled', 'bind', 'port', 'lan', 'routes'} <= osc.keys()
                assert osc['port'] == port and osc['bind'] == '127.0.0.1'
                assert osc['lan'] is False
                routes = osc['routes']
                if len(routes) == 1 and predicate(routes[0]):
                    assert osc['enabled'] == (routes[0]['state'] != 'port-busy')
                    assert (osc.get('last_error') == 'port-busy') == (
                        routes[0]['state'] == 'port-busy')
                    return routes[0]
            raise AssertionError('No matching OSC route status: ' + osc_engine.stderr())

        failure = osc_route_when(lambda route: route['state'] == 'port-busy')
        assert failure['port'] == port and failure['received'] == 0 and failure['dropped'] == 0
        assert failure['module'] == '9'
        invalid = json.loads(json.dumps(osc_patch))
        invalid['modules'][1]['config']['port'] = 65536
        assert osc_engine.call('loadGraph', False, patch=invalid)['gen'] == gen
        # A refusal must retain the same committed busy route and generation.
        assert osc_route_when(lambda route: route['state'] == 'port-busy')['module'] == '9'
        occupied.close()
        # Recommit must clean up the old listener and retry the same port.
        osc_engine.call('loadGraph', patch=minimal())
        osc_engine.call('loadGraph', patch=osc_patch)
        listening = osc_route_when(lambda route: route['state'] == 'lan-not-confirmed')
        assert listening['bind'] == '127.0.0.1' and listening['port'] == port
        packet = b'/pico1/knob/1\0'
        packet += b'\0' * ((-len(packet)) % 4)
        packet += b',f\0\0' + struct.pack('>f', 0.5)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
            sender.sendto(packet, ('127.0.0.1', port))
        assert osc_route_when(lambda route: route['received'] > 0)['received'] >= 1
        # Successful live edits must restart ingress against the newly committed
        # generation. The same listener remains localhost-only and usable.
        edited_gen = osc_engine.call('addModule', moduleId=11, moduleType='essentials.vco')['gen']
        assert edited_gen != gen
        assert osc_route_when(lambda route: route['state'] == 'lan-not-confirmed')['port'] == port

        # Removing the source must retire the worker/port before graph disposal.
        removed_gen = osc_engine.call('removeModule', moduleId=9)['gen']
        assert removed_gen != edited_gen
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            message = osc_engine.pushes.pop(0) if osc_engine.pushes else osc_engine.receive()
            if message.get('type') == 'status' and message['gen'] == removed_gen:
                assert message['status']['osc'] == dict(
                    enabled=False, bind='127.0.0.1', port=0, lan=False, routes=[])
                break
        else:
            raise AssertionError('OSC source removal did not clear status')
        # Repeated accepted reload/remove cycles must release the same port and
        # fence the prior worker generation. No non-loopback socket is opened.
        for cycle in range(4):
            reloaded_gen = osc_engine.call('loadGraph', patch=osc_patch)['gen']
            assert reloaded_gen != removed_gen
            assert osc_route_when(lambda route: route['state'] == 'lan-not-confirmed')['port'] == port
            removed_gen = osc_engine.call('loadGraph', patch=minimal())['gen']
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                message = osc_engine.pushes.pop(0) if osc_engine.pushes else osc_engine.receive()
                if message.get('type') == 'status' and message['gen'] == removed_gen:
                    assert message['status']['osc']['routes'] == []
                    break
            else:
                raise AssertionError('OSC reload churn left stale routes')
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as released:
            released.bind(('127.0.0.1', port))  # packet worker was joined/closed
        osc_engine.close()
        # The null-sink test mode additionally forces loopback even if the
        # launcher forwards --confirm-lan and the patch sets lan:true.
        confirmed = Engine(Path(osc_dir) / 'null-consent.json', '--confirm-lan')
        confirmed.call('loadGraph', patch=osc_patch)
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            message = confirmed.pushes.pop(0) if confirmed.pushes else confirmed.receive()
            if message.get('type') != 'status' or message['gen'] != confirmed.last_gen:
                continue
            osc = message['status']['osc']
            if len(osc['routes']) == 1 and osc['routes'][0]['state'] == 'lan-not-confirmed':
                assert osc['enabled'] is True and osc['lan'] is False
                assert osc['bind'] == '127.0.0.1'
                assert osc['routes'][0]['bind'] == '127.0.0.1'
                break
        else:
            raise AssertionError('Null sink did not enforce loopback-only OSC ingress')
        confirmed.close()

# Real, short, device-free end-to-end benchmark ingestion on the production sidecar.
harness = Path(__file__).resolve().parents[2] / 'tools' / 'sidecar_edit_soak.py'
smoke = subprocess.run([sys.executable, str(harness), '--engine', BINARY,
                        '--seconds', '2', '--seed', '278'], capture_output=True, text=True, timeout=30)
assert smoke.returncode == 0, smoke.stderr[-4000:] + smoke.stdout[-1200:]
summary = json.loads(smoke.stdout, object_pairs_hook=strict_object)
metrics = summary.get('performance_evidence')
retry = summary.get('wake_retry')
if retry is not None:
    assert isinstance(retry, dict) and retry.get('attempt2') == 'passed'
    first = retry.get('attempt1')
    assert isinstance(first, str) and 'overrun_wake_count=' in first and len(first) <= 3000
    # The subprocess stderr was captured even on success. Re-emit first failure
    # and clean second outcome so the Windows job logs can be audited.
    print(f'hosted Windows wake-only attempt 1 failed: {first}', file=sys.stderr)
    print('hosted Windows wake-only attempt 2 passed', file=sys.stderr)
    step_summary = os.environ.get('GITHUB_STEP_SUMMARY')
    if step_summary:
        with Path(step_summary).open('a', encoding='utf-8') as report:
            report.write('- Windows wake-only retry: attempt 1 wake-only, attempt 2 clean (count 1)\\n')
assert summary['status'] == 'passed' and summary['seed'] == 278 and summary['seconds'] == 2
assert isinstance(metrics, dict) and metrics['blocks'] >= 0.9 * 2 * 48000 / 256
assert metrics['faults'] == 0 and metrics['overruns'] == 0
print('sidecar protocol and two-second measured edit soak passed (null audio, fake MIDI only)')
