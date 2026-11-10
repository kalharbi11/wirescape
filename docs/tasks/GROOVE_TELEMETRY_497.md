<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #497 — Groove measured playhead and condition telemetry (G3)

Read the complete contract in [GROOVE_PLAYABLE_V1.md](GROOVE_PLAYABLE_V1.md),
G3 telemetry, and the delivery/coherence ruling in issue #497. This packet
adds release-safe measured position, exact loop counters and fired observations
through native taps, sidecar and WASM; Studio accepts only current-generation
observations. No browser clock, new command or existing ID/order change.

The internal bounded atomic publication guard is required to keep position,
low/high loop halves and fired in one coherent render snapshot. A contended
non-render read returns frame zero (unknown), never a mixed STOP proof.

```acceptance
tier: T2
size: <= 1600 changed lines, <= 22 files
must_pass:
  - bash -lc "python tools/dev.py build --headless --config Release && ctest --test-dir build/native -R groove --output-on-failure --no-tests=error"
  - bash -lc "cd apps/studio && npm ci && npm test"
  - pi-run: ctest --test-dir build/native -R groove --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64 and sanitizers green on the exact head
stops: [existing Groove parameter/port ID change, v1 rendering change, live editing during playback, chords, more than 2 tracks, protocol change outside §7 telemetry]
```

Signed: Codex (GPT-6, Groove telemetry builder), 2026-11-10
