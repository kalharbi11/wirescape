# SPDX-License-Identifier: GPL-3.0-only
"""Explicit manual loopback runner. Importing this module never opens a device."""
import argparse
import array
import hashlib
import json
import math
import platform
from datetime import datetime, timezone
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from analysis import analyse, pilot, stimulus
from reference import render, tone_patch

ROOT = Path(__file__).resolve().parents[2]


def is_windows():
    return sys.platform == 'win32'


def floats(values):
    data = array.array('f', values)
    if sys.byteorder != 'little':
        data.byteswap()
    return data.tobytes()


def wav(path, data, rate=48000, channels=2):
    # IEEE float WAV preserves clipping/non-finite evidence without saturation.
    fmt = struct.pack('<HHIIHHH', 3, channels, rate, rate * channels * 4, channels * 4, 32, 0)
    chunks = b'fmt ' + struct.pack('<I', len(fmt)) + fmt
    chunks += b'fact' + struct.pack('<II', 4, len(data) // (channels * 4))
    chunks += b'data' + struct.pack('<I', len(data)) + data
    path.write_bytes(b'RIFF' + struct.pack('<I', len(chunks) + 4) + b'WAVE' + chunks)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path):
    if path.stat().st_size > 512000:
        raise ValueError('configuration exceeds 512 kB')
    return json.loads(path.read_text(encoding='utf-8'))


def run_native(executable, directory):
    with (directory / 'native.log').open('w', encoding='utf-8') as log:
        child = subprocess.Popen([str(executable), '--confirm-audio', str(directory)],
                                 cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        try:
            return child.pid, child.wait(timeout=12)
        except BaseException:
            child.kill()  # Only the owned helper; never another app/process tree.
            child.wait()
            raise


def report_path(path):
    """Keep repository provenance without publishing machine-specific directories."""
    resolved = path.resolve()
    return resolved.relative_to(ROOT.resolve()).as_posix() if resolved.is_relative_to(ROOT.resolve()) else path.name


def report_error(exc, paths):
    message = str(exc) or "interrupted"
    for path in sorted((p for p in paths if isinstance(p, Path)), key=lambda p: len(str(p)), reverse=True):
        message = message.replace(str(path.resolve()), report_path(path)).replace(str(path), report_path(path))
    return message.replace(str(ROOT.resolve()), "<repository>")


def save(directory, report):
    report['files_sha256'] = {p.name: digest(p) for p in directory.iterdir()
                              if p.is_file() and p.suffix in ('.f32', '.wav', '.json')}
    (directory / 'report.json').write_text(json.dumps(report, indent=2, allow_nan=False), encoding='utf-8')
    text = ['# Wirescape capture report', '', 'Result: **' + report['status'] + '**', '',
            report['scope'], '', '```json', json.dumps(report, indent=2, allow_nan=False), '```', '']
    (directory / 'report.md').write_text('\n'.join(text), encoding='utf-8')
    print(report['status'] + ': ' + str(directory / 'report.json'))
    return 0 if report['status'] == 'pass' else 3 if report['status'] == 'unavailable' else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--confirm-audio', action='store_true', help='Authorize bounded playback and process-loopback capture')
    parser.add_argument('--stimulus', choices=('engine', 'tone', 'pulses', 'sweep'), default='engine')
    parser.add_argument('--patch', type=Path)
    parser.add_argument('--events', type=Path)
    parser.add_argument('--seconds', type=float, default=2)
    parser.add_argument('--engine', type=Path)
    parser.add_argument('--modules', type=Path)
    parser.add_argument('--native', type=Path)
    args = parser.parse_args(argv)
    if not args.confirm_audio:
        parser.error('--confirm-audio is required; no device or file has been opened')
    if not 0.1 <= args.seconds <= 5 or (args.stimulus != 'engine' and (args.patch or args.events)):
        parser.error('duration must be 0.1–5 seconds; patches/events require --stimulus engine')
    base = ROOT / 'build/native' / ('Release' if is_windows() else '')
    engine = args.engine or base / ('wirescape_engine.dll' if is_windows() else 'libwirescape_engine.so')
    modules, native = args.modules or base, args.native or base / 'wirescape_capture.exe'
    reports_root = ROOT.resolve() / 'build_reports'
    parent = reports_root / 'capture'
    if not parent.resolve().is_relative_to(reports_root):
        parser.error('report directory must not redirect outside build_reports')
    parent.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix='run-', dir=parent))
    report = dict(schema_version=1, created_utc=datetime.now(timezone.utc).isoformat(),
                  os_version=platform.platform(), python=sys.version.split()[0], status='application_fault', scope='Offline engine reference and isolated helper-process render stream only; '
                  'not speakers, endpoint post-processing, GUI live callbacks or PortAudio. No perceptual certification.',
                  application_faults=[], live_application_faults='not measured', transport_device_counters={},
                  capture_failure=None, signal_mismatch=[], config=vars(args).copy(), platform=sys.platform)
    report['config'] = {k: report_path(v) if isinstance(v, Path) else v for k, v in report['config'].items()}
    try:
        patch = (read_json(args.patch) if args.patch else tone_patch()) if args.stimulus == 'engine' else None
        events = read_json(args.events) if args.events else []
        if args.stimulus == 'engine':
            data, libraries = render(engine, modules, patch, events, args.seconds)
            report['binary_sha256'] = {report_path(p): digest(p) for p in [engine] + libraries}
        else:
            data = stimulus(args.stimulus, args.seconds)
        mono = pilot(data)
        if any(not math.isfinite(v) or abs(v) > 0.05 for v in mono):
            raise ValueError('reference exceeds finite low-level ceiling of 0.05')
        right = [-v if i < 1024 else v for i, v in enumerate(mono)]
        reference = floats(v for pair in zip(mono, right) for v in pair)
        (directory / 'reference.f32').write_bytes(reference)
        wav(directory / 'reference.wav', reference)
        (directory / 'config.json').write_text(json.dumps(dict(patch=patch, events=events, stimulus=args.stimulus,
            seconds=args.seconds, rate=48000, engine_gain=0.05 if args.stimulus == 'engine' else None, pilot_seed=731, right_pilot_inverted=True), indent=2), encoding='utf-8')
        report['commit'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    except Exception as exc:
        report['application_faults'].append(report_error(exc, [engine, modules, native, args.patch, args.events]))
        return save(directory, report)
    if not is_windows() or not native.is_file():
        report.update(status='unavailable', capture_failure='Windows build 20348+ and the compiled manual helper are required')
        return save(directory, report)
    try:
        report['capture_binary_sha256'] = digest(native)
        pid, code = run_native(native.resolve(), directory)
        meta = read_json(directory / 'native.json')
        meta['message'] = report_error(meta.get('message', ''), [directory, engine, modules, native, args.patch, args.events]) if meta.get('message') else ''
        report['capture'] = meta
        if meta['status'] == 'unavailable' and code == 3:
            report.update(status='unavailable', capture_failure=meta['message'])
            return save(directory, report)
        if meta['status'] == 'application_fault' and code != 0:
            report['application_faults'].append(meta['message'])
            return save(directory, report)
        if meta.get('isolation') != 'process_tree' or meta.get('target_pid') != pid:
            report.update(status='ambiguous', capture_failure='capture process isolation could not be established')
            return save(directory, report)
        if code != 0 or meta['status'] != 'captured' or not meta.get('stopped_cleanly'):
            raise ValueError('capture or stream cleanup failed: ' + meta.get('message', ''))
        file = directory / 'capture.f32'
        if not 0 < file.stat().st_size <= 48000 * 8 * 8 or file.stat().st_size % 8:
            raise ValueError('capture storage bound or frame alignment invalid')
        raw = file.read_bytes()
        if meta['rate'] != 48000 or meta['channels'] != 2 or meta['frames'] * 8 != len(raw):
            raise ValueError('capture format/length disagrees with metadata')
        wav(directory / 'capture.wav', raw)
        captured = array.array('f'); captured.frombytes(raw)
        if sys.byteorder != 'little': captured.byteswap()
        channel_reports = [analyse(ref, list(captured[ch::2]), meta['rate']) for ch, ref in enumerate((mono, right))]
        report['channel_map'] = ['reference L -> capture 0', 'reference R -> capture 1']
        report['signal_analysis'] = channel_reports
        report['signal_mismatch'] = [dict(channel=i, failures=r['failures']) for i, r in enumerate(channel_reports) if r['status'] != 'pass']
        counters = meta['transport_counters']
        required = {'discontinuities', 'timestamp_errors', 'position_gaps', 'render_starvations'}
        if not required <= counters.keys() or any(type(v) is not int or v < 0 for v in counters.values()):
            raise ValueError('missing or invalid transport counters')
        report['transport_device_counters'] = counters
        report['status'] = ('transport_failure' if any(counters.values()) else
                            'signal_mismatch' if any(r['status'] != 'pass' for r in channel_reports) else 'pass')
    except (Exception, KeyboardInterrupt) as exc:
        report.update(status='capture_failure', capture_failure=report_error(exc, [engine, modules, native, args.patch, args.events]))
    return save(directory, report)


if __name__ == '__main__':
    raise SystemExit(main())
