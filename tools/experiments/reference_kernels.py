"""Bounded offline CPython-reference versus NumPy kernel experiment."""
from __future__ import annotations

import argparse
import importlib.util
import json
import platform
import random
import shutil
import statistics
import sys
import time
from pathlib import Path

try:
    import numpy as np
except ImportError:
    np = None

RATE = 48_000
SEED = 0x57495245
DELAY = 37
REPEATS_PER_MEASURED_BLOCK = 16


def mix_py(a, b, gain, out):
    for i in range(len(out)):
        out[i] = a[i] * gain + b[i]


def ramp_py(start, end, out):
    n = len(out)
    for i in range(n):
        out[i] = start + (end - start) * (i / n)


def delay_py(source, state, cursor, out):
    size = len(state)
    for i, sample in enumerate(source):
        out[i] = state[cursor]
        state[cursor] = sample
        cursor = (cursor + 1) % size
    return cursor


def mix_np(a, b, gain, out):
    np.multiply(a, gain, out=out)
    np.add(out, b, out=out)


def ramp_np(start, end, indices, out):
    np.multiply(indices, (end - start) / len(out), out=out)
    np.add(out, start, out=out)


def delay_np(source, state, cursor, out):
    size = len(state)
    for i in range(len(source)):
        out[i] = state[cursor]
        state[cursor] = source[i]
        cursor = (cursor + 1) % size
    return cursor


def percentile(values, p):
    values = sorted(values)
    pos = (len(values) - 1) * p
    lo = int(pos)
    hi = min(lo + 1, len(values) - 1)
    return values[lo] + (values[hi] - values[lo]) * (pos - lo)


def make_vectors(frames, numpy):
    rng = random.Random(SEED)
    a = [rng.uniform(-1, 1) for _ in range(frames)]
    b = [rng.uniform(-1, 1) for _ in range(frames)]
    if numpy:
        return (np.asarray(a, dtype=np.float64), np.asarray(b, dtype=np.float64),
                np.zeros(frames, dtype=np.float64), np.arange(frames, dtype=np.float64))
    return a, b, [0.0] * frames, list(range(frames))


def candidate_run(candidate, frames, blocks, warmup):
    use_np = candidate == "numpy"
    a, b, out, indices = make_vectors(frames, use_np)
    gain, start, end = .375, -.25, .75
    funcs = {
        "two_input_gain_mix": lambda: (mix_np if use_np else mix_py)(a, b, gain, out),
        "sample_count_linear_ramp": lambda: (ramp_np(start, end, indices, out) if use_np else ramp_py(start, end, out)),
    }
    state = np.zeros(DELAY, dtype=np.float64) if use_np else [0.0] * DELAY
    cursor = 0
    def run_delay():
        nonlocal cursor
        cursor = (delay_np if use_np else delay_py)(a, state, cursor, out)
    funcs["integer_ring_delay_no_feedback"] = run_delay
    results = {}
    for name, call in funcs.items():
        if name == "integer_ring_delay_no_feedback":
            if use_np:
                state.fill(0.0)
            else:
                state[:] = [0.0] * DELAY
            cursor = 0
        for _ in range(warmup):
            call()
        wall, cpu = [], []
        t_cpu, t_wall = time.process_time(), time.perf_counter()
        for _ in range(blocks):
            c0, w0 = time.process_time(), time.perf_counter()
            for _ in range(REPEATS_PER_MEASURED_BLOCK):
                call()
            cpu.append((time.process_time() - c0) / REPEATS_PER_MEASURED_BLOCK)
            wall.append((time.perf_counter() - w0) / REPEATS_PER_MEASURED_BLOCK)
        wall_total = time.perf_counter() - t_wall
        cpu_total = time.process_time() - t_cpu
        audio_seconds = frames * blocks * REPEATS_PER_MEASURED_BLOCK / RATE
        results[name] = {
            "frames": frames, "measured_block_batches": blocks, "repeats_per_batch": REPEATS_PER_MEASURED_BLOCK, "measured_kernel_blocks": blocks * REPEATS_PER_MEASURED_BLOCK, "warmup_blocks": warmup,
            "wall_seconds_total": wall_total,
            "process_cpu_seconds_total": cpu_total,
            "wall_seconds_per_batch_average_block": {"p50": statistics.median(wall), "p95": percentile(wall, .95), "p99": percentile(wall, .99), "max": max(wall)},
            "process_cpu_seconds_per_batch_average_block": {"p50": statistics.median(cpu), "p95": percentile(cpu, .95), "p99": percentile(cpu, .99), "max": max(cpu)},
            "process_cpu_seconds_per_audio_second": cpu_total / audio_seconds,
            "output_sum": float(sum(out)),
        }
    return results


def validate_correctness(use_numpy):
    a, b, out, indices = make_vectors(64, False)
    expected = [0.0] * 64
    mix_py(a, b, .375, expected)
    if use_numpy:
        actual = np.zeros(64, dtype=np.float64)
        mix_np(np.asarray(a), np.asarray(b), .375, actual)
        assert all(abs(x - float(y)) <= 1e-14 for x, y in zip(expected, actual))
    ramp_py(-.25, .75, expected)
    if use_numpy:
        ramp_np(-.25, .75, np.arange(64, dtype=np.float64), actual)
        assert all(abs(x - float(y)) <= 1e-14 for x, y in zip(expected, actual))
    source = [float(i + 1) for i in range(211)]
    flat_state, flat_out = [0.0] * DELAY, [0.0] * len(source)
    delay_py(source, flat_state, 0, flat_out)
    assert flat_out[:DELAY] == [0.0] * DELAY and flat_out[DELAY:] == source[:-DELAY]
    class View:
        def __init__(self, backing, lo, hi): self.backing, self.lo, self.hi = backing, lo, hi
        def __len__(self): return self.hi - self.lo
        def __getitem__(self, i): return self.backing[self.lo + i]
        def __setitem__(self, i, value): self.backing[self.lo + i] = value
    partition_state, partition_cursor, partition_out = [0.0] * DELAY, 0, [0.0] * len(source)
    for lo, hi in ((0, 13), (13, 88), (88, 211)):
        partition_cursor = delay_py(source[lo:hi], partition_state, partition_cursor, View(partition_out, lo, hi))
    assert partition_out == flat_out and partition_state == flat_state and partition_cursor == len(source) % DELAY
    if use_numpy:
        ns, no = np.zeros(DELAY), np.zeros(len(source))
        delay_np(np.asarray(source), ns, 0, no)
        assert no.tolist() == flat_out and ns.tolist() == flat_state
    return {"passed": True, "delay_frames": len(source), "delay_partitions": [13, 75, 123], "numpy_compared": use_numpy}


def run(frames=512, blocks=128, warmup=8, report_path=None):
    if not 1 <= frames <= 512 or not 1 <= blocks <= 128 or not 0 <= warmup <= 128:
        raise ValueError("frames 1..512, blocks 1..128, warmup 0..128")
    candidates = ["cpython_reference"] + (["numpy"] if np is not None else [])
    correctness = validate_correctness(np is not None)
    report = {
        "experiment": "bounded_reference_kernels", "seed": SEED,
        "environment": {"python": sys.version, "implementation": platform.python_implementation(), "platform": platform.platform(), "machine": platform.machine(), "process_cpu_clock": {"implementation": time.get_clock_info("process_time").implementation, "reported_resolution_seconds": time.get_clock_info("process_time").resolution}, "numpy": getattr(np, "__version__", None), "optional_runtime_availability": {"pypy": shutil.which("pypy3") is not None or shutil.which("pypy") is not None, "numba": importlib.util.find_spec("numba") is not None, "Cython": importlib.util.find_spec("Cython") is not None}},
        "correctness_validation_before_timing": correctness,
        "contract": {"sample_rate_hz": RATE, "precision": "float64 (Python binary64 / NumPy float64)", "frames_per_block": frames, "measured_block_batches": blocks, "repeats_per_batch": REPEATS_PER_MEASURED_BLOCK, "measured_kernel_blocks": blocks * REPEATS_PER_MEASURED_BLOCK, "warmup_blocks": warmup, "preallocated_inplace_output": True, "kernels": {"two_input_gain_mix": "out[i] = a[i] * 0.375 + b[i]", "sample_count_linear_ramp": "stateless block fill: out[i] = -0.25 + (0.75 - -0.25) * i / frames; endpoint excluded; no cross-block ramp state is modeled", "integer_ring_delay_no_feedback": f"zero-initialized {DELAY}-sample delay; output old ring value then overwrite with input; state persists across blocks; no feedback"}},
        "candidates": candidates,
        "results": {c: candidate_run(c, frames, blocks, warmup) for c in candidates},
        "limitations": ["NumPy mix/ramp use in-place multipass ufuncs; ramp indices are preallocated.", "NumPy ring delay uses scalar state updates and is not a vectorized delay implementation.", "On this Windows host, observed process CPU increments are coarse relative to the very short candidates; zero or one-tick CPU values are timer-limited and should not rank candidates. Wall-time percentiles remain the more useful result here.", "No native comparison, audio device, latency, realtime stability, or capacity conclusion."],
    }
    if report_path:
        path = Path(report_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frames", type=int, default=512)
    parser.add_argument("--blocks", type=int, default=128)
    parser.add_argument("--warmup", type=int, default=8)
    parser.add_argument("--report", default="build_reports/experiments/reference-kernels.json")
    args = parser.parse_args(argv)
    report = run(args.frames, args.blocks, args.warmup, args.report)
    print(f"candidates={','.join(report['candidates'])}; frames={args.frames}; blocks={args.blocks}; report={args.report}")
    for candidate, kernels in report["results"].items():
        for kernel, metrics in kernels.items():
            print(f"{candidate}/{kernel}: CPU/audio-s={metrics['process_cpu_seconds_per_audio_second']:.6g}; batch-average-wall-p95={metrics['wall_seconds_per_batch_average_block']['p95']:.6g}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
