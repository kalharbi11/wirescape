<!-- SPDX-License-Identifier: GPL-3.0-only -->
# 2C-b honest callback load and dropout status (#292)

Read #292 and its latest handoff, AGENTS.md, ENGINE_PROTOCOL status,
native/io/audio/{callback_render.h,portaudio_service.cpp}, Session::audio_status,
and Studio StatusBar. Branch task/292-2cb. Tier T2.

Measure live callback render/interleave wall time divided by that callback's
actual frame period. Publish one lock-free float; no smoothing, artificial cap,
allocation, locking or JSON in the callback. Count only PortAudio output-underflow
flags as dropouts, never infer them from load. Retain the final callback's count
after proven shutdown and accumulate it across stream restarts.

Before the first measured callback, use existing starting state and numeric zero
on the wire; Studio displays CPU not available. No callback/device remains
unavailable. Preserve protocol v0 and field meanings; no optional keys or ABI edits.
Null-sink histogram is separate from physical callback performance.

The existing callback regression covers measured loads, variable frame counts,
overloads above 100%, underflow counts and allocation-free publication. Studio's
mock bridge covers starting/running/lost states, load/dropouts and absent data.
The --telemetry-bench mode compares identical 32-VCO graphs with 64 distinct
param/output taps or none; prints five alternating 800-block timing repetitions.
It is measurement only: Keel must identify the tap-cost tolerance/aggregation
before that Pi comparison is certified. See #292's T2 batch preflight comment.
Never reinterpret ABI2/ABI1 or polyphony gates as tap-cost thresholds silently.

Eight scoped files use the workflow's +50% file-budget allowance (original 7);
changed-line budget and all existing numerical limits remain unchanged.

```acceptance
tier: T2
size: <= 350 changed lines, <= 8 files
must_pass:
  - ctest --test-dir build/native -R "audio|sidecar_" --output-on-failure --no-tests=error
  - ci: cd apps/studio && npm run build && npx playwright test tests/mock-bridge/real-engine.spec.ts
  - pi-run: ctest --test-dir build/native -R abi2_audio_callback_live_midi_tests --output-on-failure --no-tests=error
  - pi-run: build/native/audio_callback_live_midi_tests --telemetry-bench
stops: [ABI header edit, ID/format change, new dependency, real device access, threshold loosening]
```

Further gates: Keel exact-head T2 review; completed exact-head Pi evidence;
64-tap comparison evaluated against Keel's named tolerance (not command exit alone).

Signed: Codex Lead (Codex), 2026-11-08
