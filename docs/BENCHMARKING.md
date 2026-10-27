<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Device-free engine measurements

Build Release headlessly with `python tools/dev.py build --headless`, then run
`python tools/dev.py test`. These measurements never open audio or MIDI devices.
Use the same compiler, flags, rate and block size when comparing revisions; finish
builds before collecting timings. Keep JSON/logs outside tracked files and attach
results to the PR. Hosted CPU scheduling can affect wall-clock results.

## Benchmark comparison

The ABI2 benchmark writes `build_reports/abi2/benchmark.json`. It interleaves five
repetitions and reports their per-case means. Compare copies from two revisions:

```sh
python tools/bench/compare_bench.py --base before.json --head after.json
python tools/bench/compare_bench.py --head after.json --gate
```

The table computes each median from `repetition_means_ns`, recomputes ratios from
those medians, and shows `(head/base - 1) * 100%` deltas. `--gate` returns 1 if
mono exceeds 1.05 or poly8/eight-mono exceeds 1.00; equality passes. Malformed
input returns 2. Legacy fixed-order benchmark means are refused: they cannot
establish the median gate. Until M1-3 merges, use its benchmark for these inputs.
The modulated ratio is reported, not thresholded by this tool.

## Deterministic instruction cost

Once M1-3 is merged, with Valgrind installed, run:

```sh
python tools/bench/abi2_cost.py --help
python tools/bench/abi2_cost.py --build-dir build/native --modules-dir build/native
```

Use the probe's Release executable and matching module directory (on Windows,
executables/modules may be under `Release`). The helper subtracts shorter from
longer callgrind runs to remove startup cost and reports Ir per 256-frame block.
Do not compare sanitized instruction counts with Release counts. Wall-clock and
instruction-count results answer different questions; retain both.

## Paced soak and voice budget

```sh
build/native/wirescape_soak --graph poly8 --rate 48000 --block 128 --seconds 600
build/native/wirescape_soak --graph six_essentials_shim --modules build/native --seconds 600
build/native/wirescape_soak --budget --rate 48000 --block 128 --seconds 10
```

Graphs `mono`, `poly8` and `voices:1` through `voices:16` are VCO → VCF → VCA,
with an ADSR driving VCA gain. Each voice gets a sustained note. The shim graph
hardcodes the eight modules, seven cables and parameters of the Six Essentials
demo; it does not exercise a document loader. `--modules` selects its built ABI1
module libraries. `--budget` tests all 1–16 voice counts, for `--seconds` **each**,
and reports the largest whose p99 render time is strictly below half the period;
zero means none. It is a bound for this graph and machine, not a product promise.

After 256 warmup blocks, callbacks follow absolute steady-clock deadlines with
`sleep_until`; no device or CPU/governor settings are changed. JSON includes
configuration, block count, render-time and thread-CPU min/median/p99/p99.9/max,
counts strictly above 50%/100% of the block period, `preempted` blocks where
wall time minus thread CPU time exceeds half a period, deadline misses,
allocations and faults. Linux uses `CLOCK_THREAD_CPUTIME_ID`; Windows uses
`GetThreadTimes`.
Percentiles use nearest rank; even-sized medians average the middle pair. Render
timing includes the allocation hook and error check, but excludes output checks
and pacing. A late deadline includes scheduler delays and is distinct from render
cost. No render allocations, faults, nonfinite samples or entirely silent runs are
accepted. Samples are preallocated (at most five million blocks per run).

For a Linux ARM64 board, copy/build the same revision with a user-local toolchain,
then run the six combinations of `poly8` / `six_essentials_shim` and block sizes
64 / 128 / 256 at 48 kHz for 600 seconds each, sequentially. Also run `--budget`
at 48 kHz/128. Record commit, compiler, CMake, `uname -r` and the read-only governor
observation (or unavailable) beside every table. Never use sudo, install system
packages, change the governor/kernel, or open devices on the board. Toolchains
belong in user storage or temporary directories. CTest's one-second smoke and CLI
tests have no timing thresholds; the long measurements supply separate evidence.
