# SPDX-License-Identifier: GPL-3.0-only
"""Offline Pi acceptance for two four-voice CHAOS-lite instances."""
import json
import math
from pathlib import Path
import subprocess
import sys


def validate_result(result):
    try:
        worst = float(result['render_ns']['max'])
        peak = float(result['peak'])
        valid = (result['sample_rate'] == 48000 and result['block'] == 128
                 and result['seconds'] == 120 and result['blocks'] > 0
                 and result['allocations'] == 0 and result['faults'] == 0
                 and result['over_100_percent'] == 0 and math.isfinite(peak) and peak > 0
                 and math.isfinite(worst) and 0 <= worst < (1e9 * 128 / 48000) * .5)
    except (KeyError, TypeError, ValueError, OverflowError) as error:
        raise ValueError('Missing or invalid CHAOS timing evidence') from error
    if not valid:
        raise ValueError('CHAOS acceptance failed: require 48k/128, 120s, finite non-silence, '
                         'zero allocations/faults and worst render below 50% of the period')


def main():
    root = Path(__file__).resolve().parents[1]
    binary = root / 'build/native' / ('wirescape_soak.exe' if sys.platform == 'win32' else 'wirescape_soak')
    command = [str(binary), '--patch', str(root / 'native/examples/chaos_two.json'),
               '--rate', '48000', '--block', '128', '--seconds', '120']
    run = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=180)
    if run.returncode:
        raise RuntimeError(run.stderr[-6000:] or run.stdout[-6000:] or 'CHAOS soak failed')
    result = json.loads(run.stdout)
    print(json.dumps(result, indent=2))
    validate_result(result)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
