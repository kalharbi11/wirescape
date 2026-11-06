# SPDX-License-Identifier: GPL-3.0-only
"""Offline Pi CPU acceptance for one built-in module (Track S): two instances, 48 kHz / 128 frames.

Runs build/native/abi2_module_cpu_probe (Qt-free, device-free, deterministic inputs: a saw into A,
seeded noise into B) and passes when the worst block is under half the period with zero
allocations and zero faults. Usage: python tools/module_cpu_probe.py sound.spectral [--seconds S]
"""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys

PREFIX = 'module_cpu_probe '
RATE, BLOCK, INSTANCES = 48000, 128, 2


def parse_line(output):
    for line in output.splitlines():
        if line.startswith(PREFIX):
            return json.loads(line[len(PREFIX):])
    raise ValueError('probe printed no module_cpu_probe line')


def validate_result(result, module, seconds):
    try:
        worst = float(result['render_ns']['max'])
        p99 = float(result['render_ns']['p99'])
        peak = float(result['peak'])
        valid = (result['module'] == module and result['instances'] == INSTANCES
                 and result['sample_rate'] == RATE and result['block'] == BLOCK
                 and float(result['seconds']) == float(seconds) and result['blocks'] > 0
                 and result['allocations'] == 0 and result['faults'] == 0
                 and math.isfinite(peak) and peak > 0 and math.isfinite(p99) and 0 <= p99 <= worst
                 and math.isfinite(worst) and worst < 1e9 * BLOCK / RATE * .5)
    except (KeyError, TypeError, ValueError, OverflowError) as error:
        raise ValueError('missing or invalid module CPU evidence') from error
    if not valid:
        raise ValueError(f'{module} CPU acceptance failed: require {INSTANCES} instances at 48k/128, '
                         'zero allocations/faults, non-silent finite output and worst block below 50% of the period')


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('module', help='built-in module ID, e.g. sound.spectral')
    parser.add_argument('--seconds', type=float, default=60.0)
    args = parser.parse_args(argv)
    if not args.seconds > 0:
        parser.error('--seconds must be positive')
    return args


def main(argv=None):
    args = parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    binary = root / 'build/native' / ('abi2_module_cpu_probe.exe' if sys.platform == 'win32' else 'abi2_module_cpu_probe')
    command = [str(binary), '--module', args.module, '--instances', str(INSTANCES), '--rate', str(RATE),
               '--block', str(BLOCK), '--seconds', str(args.seconds)]
    run = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=args.seconds * 4 + 60)
    if run.returncode:
        raise RuntimeError(run.stderr[-4000:] or run.stdout[-4000:] or 'module CPU probe failed')
    result = parse_line(run.stdout)
    print(PREFIX + json.dumps(result))
    validate_result(result, args.module, args.seconds)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
