#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
config="$HOME/.config/wirescape/demo.env"
# This owner-created file is shell syntax, with quoted values (see example).
if [ -f "$config" ]; then set -a; . "$config"; set +a; fi
exec python3 -I - "$root" "$@" <<'PY'
import fcntl
import glob
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time

root = Path(sys.argv[1])
args = sys.argv[2:]
dry = bool(args and args[0] == '--dry-run')
if dry:
    args = args[1:]
if len(args) != 1 or args[0] not in ('1', '2', '3', 'stop', 'status') or (dry and args[0] in ('stop', 'status')):
    raise SystemExit('Usage: demo.sh [--dry-run] 1|2|3 | stop | status')
action = args[0]
cache = Path.home() / '.cache/wirescape'
cache.mkdir(parents=True, exist_ok=True, mode=0o700)
lock = (cache / 'demo.lock').open('a')
fcntl.flock(lock, fcntl.LOCK_EX)
state_file = cache / 'demo-pids.json'
try:
    state = json.loads(state_file.read_text())
except FileNotFoundError:
    state = {}

def stamp(pid):
    try:
        # /proc stat command name may contain spaces or parentheses.
        stat = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
        return None if stat[0] == 'Z' else stat[19]
    except (FileNotFoundError, ProcessLookupError):
        return None

def alive(record):
    return isinstance(record, dict) and record.get('start') is not None and stamp(record['pid']) == record['start']

def save():
    temp = state_file.with_suffix('.tmp')
    temp.write_text(json.dumps(state))
    temp.replace(state_file)

def stop(name):
    record = state.pop(name, None)
    if alive(record):
        os.kill(record['pid'], signal.SIGINT if name == 'player' else signal.SIGTERM)
        deadline = time.monotonic() + 3
        while alive(record) and time.monotonic() < deadline:
            time.sleep(.05)
        if alive(record):
            os.kill(record['pid'], signal.SIGKILL)
    save()

if action == 'status':
    for name in ('player', 'server', 'kiosk'):
        record = state.get(name)
        print(name, record['pid'] if alive(record) else 'stopped')
    log = cache / 'demo.log'
    if log.exists():
        with log.open('rb') as stream:
            stream.seek(max(0, log.stat().st_size - 4096))
            lines = stream.read().decode(errors='replace').splitlines()
        if lines:
            print(lines[-1])
    raise SystemExit(0)
if action == 'stop':
    for name in ('player', 'kiosk', 'server'):
        stop(name)
    raise SystemExit(0)

matches = glob.glob(str(root / f'native/examples/class_demo_{action}_*.json'))
if len(matches) != 1:
    raise SystemExit('Expected exactly one class demo patch; install P0A patches first')
patch = json.loads(Path(matches[0]).read_text())
ports = []
for item in filter(None, os.environ.get('MIDI_PORTS', '').split(';')):
    name, separator, system = item.partition('|')
    if not separator or not name or not system:
        raise SystemExit('MIDI_PORTS entries must be name|system_name')
    ports.append(dict(kind='midi', name=name, system_name=system, enabled=True))
if len(ports) > 16:
    raise SystemExit('At most 16 MIDI ports')
patch['devices'] = ports
block = os.environ.get('BLOCK', '256')
if not block.isdecimal() or not 1 <= int(block) <= 4096:
    raise SystemExit('BLOCK must be an integer from 1 to 4096')
cli = os.environ.get('CLI', str(root / 'build/native/wirescape_cli'))
ui = Path(os.environ.get('UI_DIST', str(root / 'apps/studio/dist')))
chromium = os.environ.get('CHROMIUM') or shutil.which('chromium-browser') or shutil.which('chromium') or 'chromium'
copy = cache / f'class_demo_{action}.json'
player = [cli, 'play', str(copy), '--confirm-audio', '--confirm-midi', '--device',
          os.environ.get('AUDIO_DEVICE', ''), '--rate', '48000', '--block', block]
server = ['python3', '-I', '-m', 'http.server', '4173', '--bind', '127.0.0.1', '--directory', str(ui)]
kiosk = [chromium, '--user-data-dir=' + str(cache / 'chromium'), '--kiosk', '--app=http://127.0.0.1:4173']
if dry:
    print(json.dumps(dict(patch_path=str(copy), patch=patch, player=player, server=server, kiosk=kiosk)))
    raise SystemExit(0)
if not Path(cli).is_file() or not os.access(cli, os.X_OK) or not (ui / 'index.html').is_file():
    raise SystemExit('Build CLI and UI first; check CLI and UI_DIST in demo.env')
if not player[6] or not ports or not shutil.which(chromium):
    raise SystemExit('Set AUDIO_DEVICE, MIDI_PORTS and an available CHROMIUM in demo.env')
copy.write_text(json.dumps(patch, indent=2) + '\n')

def start(name, command):
    if alive(state.get(name)):
        return
    with (cache / ('demo.log' if name == 'player' else f'demo-{name}.log')).open('ab') as log:
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True)
    state[name] = dict(pid=process.pid, start=stamp(process.pid))
    save()
    time.sleep(.2)
    if process.poll() is not None:
        raise RuntimeError(f'{name} exited; see {cache}/demo*.log')

stop('player')
try:
    start('server', server)
    start('kiosk', kiosk)
    start('player', player)
except BaseException:
    for name in ('player', 'kiosk', 'server'):
        stop(name)
    raise
print(f'Demo {action} started; demo.sh stop to stop it')
PY
