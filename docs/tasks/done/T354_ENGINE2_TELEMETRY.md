<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #354 A1: engine2 render-owned telemetry taps

**Shared brief:** `docs/tasks/T350_ENGINE_TELEMETRY.md` (#350).
**Keel's authority:** `docs/design/ENGINE_TELEMETRY.md`, rulings 1–4, 6 and 12.
**Issue:** #354. **Tier:** T2.

Implement only the native/engine2 tap substrate; the sidecar and wasm telemetry
consumers belong to #357 and #358. Add a bounded quiescent candidate-side
`set_telemetry_taps` (default capacity 256), a control-owner-only
`read_telemetry`, relaxed atomic per-value/count publication with a release
end-of-block frame stamp, event output counters in `Impl::emit` via precomputed
slot indices, and adoption of the new layout inside the existing replan park.
No tap adds zero work; do not access module-internal DSP state.

**Read first:** `native/engine2/engine2.h`; `native/engine2/engine2.cpp`
(parameter construction around `modulation_sum`, `Impl::emit`, `replan`);
`native/engine2/tests/engine_tests.cpp`; `native/tests2/live_replan_tests.cpp`.

**Unit evidence:** effective param after modulation differs from base, amount
zero is base, untapped data is absent, event output counts survive a burst,
envelope-style outputs reduce to max over channels, retained-node taps follow
replan and removed-node taps disappear; invalid nodes, kinds and indices refuse.
Check no allocations in render.

**Hosted requirements:** CI `engine`, `sanitizers` and `linux-arm64`;
cross-family Keel review with `Keel: T2 review please, head <sha7>`.

```acceptance
tier: T2
size: <= 400 changed lines, <= 6 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
stops: [lock/allocation/I/O/try in render, change to replan park/swap beyond the tap layout, ABI or SDK header edit, reading module-internal DSP state]
```

Signed: ChatGPT (GPT-6, engine2 A1 builder), 2026-11-08
