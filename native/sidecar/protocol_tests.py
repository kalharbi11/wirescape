# SPDX-License-Identifier: GPL-3.0-only
"""Process conformance: pipes, null sink and an explicitly fake device catalogue."""
import atexit
import json
import queue
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
            try:
                for line in self.process.stdout:
                    assert len(line.rstrip(b'\n')) <= 2097152
                    assert line.endswith(b'\n')
                    self.messages.put(json.loads(line, object_pairs_hook=strict_object))
            except Exception as error:
                self.errors.append(error)
        atexit.register(lambda: self.process.kill() if self.process.poll() is None else None)
        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()
        try:
            self.hello = self.messages.get(timeout=10)
        except queue.Empty as error:
            exit_code = self.process.poll()
            if exit_code is None:
                raise AssertionError('engine is still running but emitted no hello within 10s') from error
            stderr = self.process.stderr.read().decode(errors='replace')[-4096:]
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

    def receive(self):
        try:
            message = self.messages.get(timeout=10)
        except queue.Empty as error:
            exit_code = self.process.poll()
            if exit_code is None:
                raise AssertionError('engine is still running but emitted no reply within 10s') from error
            stderr = self.process.stderr.read().decode(errors='replace')[-4096:]
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
            assert self.process.wait(timeout=10) == 0, self.process.stderr.read().decode()
        finally:
            if self.process.poll() is None:
                self.process.kill()
            self.reader.join(timeout=5)
        assert not self.errors, self.errors


def minimal():
    return dict(format='wirescape.native.patch', version=2,
                modules=[dict(id=7, module='essentials.output')], connections=[],
                ui=dict(labels={'7': 'Out'}, opaque={'unchanged': True}))


def edge(kind, source, output, dest, port, **extra):
    return dict(kind=kind, **{'from': dict(module=source, port=output),
                            'to': dict(module=dest, port=port)}, **extra)


with tempfile.TemporaryDirectory() as directory:
    settings = Path(directory) / 'settings.json'
    engine = Engine(settings, '--confirm-audio', '--confirm-midi')  # null overrides both
    patch = minimal()
    original = json.dumps(patch)
    assert engine.call('loadGraph', patch=patch)['gen'] == '1'
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
            assert all(telemetry[key] == {} for key in ['effective', 'phase', 'envelope', 'scope', 'eventActivity'])
            if telemetry['meters'].get('7', [0])[0] > 0:
                break
    else:
        raise AssertionError('no measured null-sink output')
    for path in sorted(Path(EXAMPLES).glob('class_demo_*.json')):
        engine.call('loadGraph', patch=json.loads(path.read_text()))
        engine.call('noteOn', note=60, velocity=0.8)
    # Successful loadGraph resets the incremental ID namespace.
    engine.call('loadGraph', patch=minimal())
    engine.call('addModule', moduleId=10, moduleType='essentials.lfo')
    old_session = engine.session
    engine.close(b'{"unfinished":')
    restarted = Engine(settings)
    assert restarted.session != old_session
    assert restarted.call('loadGraph', patch=minimal())['gen'] == '1'
    restarted.close()
    # Long block periods cannot delay hello or EOF teardown. 8 kHz x 4096 frames is the
    # longest supported period (~512 ms); the engine refuses rates below 8 kHz.
    started = time.monotonic()
    slow = Engine(Path(directory) / 'slow.json', '--rate', '8000', '--block', '4096')
    assert time.monotonic() - started < 2.0, 'hello waited on block-paced work'
    slow.call('loadGraph', patch=minimal())
    closing = time.monotonic()
    slow.close()
    assert time.monotonic() - closing < 2.0, 'EOF teardown waited on block-paced work'
    for flags in [('--unknown',), ('--rate', '0'), ('--block', '4097'), ('--device',)]:
        result = subprocess.run([BINARY, *flags], input=b'', capture_output=True, timeout=10)
        assert result.returncode != 0 and not result.stdout
print('sidecar protocol process vectors passed (null audio, fake MIDI only)')
