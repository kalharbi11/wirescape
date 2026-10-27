# Bounded reference-kernel experiment

This small Milestone 1 preparation compares three numerical kernels using a CPython reference and NumPy when NumPy is already available. It is offline and never opens an audio device. The harness is [reference_kernels.py](../tools/experiments/reference_kernels.py); focused correctness checks are [test_reference_kernels.py](../tools/foundation/tests/test_reference_kernels.py).

## Contract

- Deterministic seed `0x57495245`; sample inputs are generated before timing.
- Python floats and NumPy arrays use binary64/float64. Output arrays/lists are allocated before timed loops and updated in place.
- Two input gain mix: `out[i] = a[i] * 0.375 + b[i]`.
- Sample-count linear control ramp: stateless block fill `out[i] = -0.25 + (0.75 - -0.25) * i / frames`; the endpoint is excluded. It does not model cross-block ramp state or partition invariance.
- Integer ring delay: 37 samples, initially zero, emits the old ring value and then stores the input, with state carried across blocks and no feedback.
- Defaults are 512 frames, 128 measured block batches, 16 kernel calls per batch, and 8 warmup blocks. Maximum frame/batch counts are bounded at 512/128. At 48 kHz, the default measured audio workload is 21.8453 seconds per candidate and kernel. The repeat factor gives process CPU clocks enough work to resolve on coarse-clock systems; reported per-block percentiles divide each batch by 16.
- Reports include process CPU and wall totals, p50/p95/p99/max across batch-average block timings, and process CPU seconds per one audio second at 48 kHz. Each sample times 16 kernel block calls and divides by 16; these percentiles describe batch-average block duration, not individual block tails or latency. Preparation is outside the timed loop. Import/module initialization is not represented as a cold-start metric.

NumPy mix uses two in-place ufunc passes and no result temporary. Its ramp uses a preallocated index vector and two in-place ufunc passes. NumPy delay intentionally uses scalar ring operations, matching reference state transitions; it is not a vectorized implementation. Python-level timer calls bracket each individual block, so tiny block timings include instrumentation overhead. No speed assertions are used.

## Run and interpret

Run `python tools/experiments/reference_kernels.py --frames 512 --blocks 128 --warmup 8`. It writes JSON to the ignored `build_reports/experiments/reference-kernels.json` by default and prints a short summary. Report metadata includes Python/platform/NumPy versions and availability probes for PyPy, Numba, and Cython; availability does not mean a runtime was exercised. Do not install missing candidates as part of this experiment.

The harness runs a tiny correctness validation before any timing. It compares mix/ramp/delay outputs within floating-point tolerance and exercises delay reset, multiple partitions, ring wraps, nonzero output, and final state equivalence. The measurement is a contained numerical microexperiment. It does not establish a Python-versus-native winner, freeze an SDK, or show device latency, realtime stability, or application capacity. Native and graph/event workloads remain future comparisons under the approved foundation plan.

On this Windows host, process CPU timing advanced in coarse ticks relative to some candidate totals despite Python reporting a fine nominal clock resolution. The JSON records the clock implementation/resolution and flags this limitation. Treat zero or one-tick CPU totals as unresolved; do not use them to rank candidates. The per-block wall percentiles are the usable comparative signal in this run.
