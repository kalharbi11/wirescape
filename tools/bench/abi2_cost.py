#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Measure deterministic Ir/block by subtracting two bounded callgrind runs."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
MODES = ('abi1_mono', 'abi2_mono', 'abi2_modulated', 'abi2_poly8')


def instructions(path):
    events = None
    for line in path.read_text().splitlines():
        if line.startswith('events:'):
            events = line.split()[1:]
        if line.startswith('summary:') and events and 'Ir' in events:
            return int(line.split()[1 + events.index('Ir')])
    raise ValueError('callgrind result has no Ir summary')


def cost(probe, modules, mode, counts, valgrind):
    totals = []
    with tempfile.TemporaryDirectory(prefix='abi2-cost-') as folder:
        for blocks in counts:
            output = Path(folder) / f'{blocks}.callgrind'
            subprocess.run([valgrind, '--tool=callgrind', '--error-exitcode=1',
                            f'--callgrind-out-file={output}', str(probe), str(modules), mode, str(blocks)],
                           cwd=ROOT, check=True, timeout=180)
            totals.append(instructions(output))
    difference = totals[1] - totals[0]
    if difference <= 0:
        raise ValueError('callgrind instruction difference must be positive')
    return dict(name=mode, ir_per_block=difference / (counts[1] - counts[0]), totals=totals)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/native')
    parser.add_argument('--modules-dir', type=Path)
    parser.add_argument('--configuration', default='Release')
    parser.add_argument('--blocks', type=int, nargs=2, default=(50, 150))
    parser.add_argument('--output', type=Path)
    parser.add_argument('--gate', action='store_true', help='require mono Ir ratio <= 1.05')
    args = parser.parse_args(argv)
    if not 0 < args.blocks[0] < args.blocks[1] <= 1000000:
        parser.error('block counts must be positive, increasing, and at most 1000000')
    valgrind = shutil.which('valgrind')
    if not valgrind:
        print('error: --gate requires valgrind' if args.gate else 'skipped: valgrind is not installed')
        return int(args.gate)
    binary = args.build_dir.resolve()
    suffix = '.exe' if sys.platform == 'win32' else ''
    probe = binary / f'abi2_cost_probe{suffix}'
    if not probe.exists():
        probe = binary / args.configuration / probe.name
    if not probe.exists():
        subprocess.run(['cmake', '-S', str(ROOT / 'native'), '-B', str(binary),
                        '-DWIRESCAPE_BUILD_UI=OFF', f'-DCMAKE_BUILD_TYPE={args.configuration}'], check=True)
        subprocess.run(['cmake', '--build', str(binary), '--config', args.configuration,
                        '--target', 'abi2_cost_probe', 'ws_vco', 'ws_vcf', 'ws_vca', '--parallel', '2'], check=True)
        probe = binary / f'abi2_cost_probe{suffix}'
        if not probe.exists():
            probe = binary / args.configuration / probe.name
    rows = [cost(probe, (args.modules_dir or probe.parent).resolve(), mode, args.blocks, valgrind) for mode in MODES]
    result = dict(block_counts=args.blocks, frames=256, rows=rows,
                  mono_ratio=rows[1]['ir_per_block'] / rows[0]['ir_per_block'],
                  modulated_ratio=rows[2]['ir_per_block'] / rows[1]['ir_per_block'])
    print('| Probe | Ir/block |\n|---|---:|')
    for row in rows:
        print(f"| {row['name']} | {row['ir_per_block']:.2f} |")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    return int(args.gate and result['mono_ratio'] > 1.05)


if __name__ == '__main__':
    raise SystemExit(main())
