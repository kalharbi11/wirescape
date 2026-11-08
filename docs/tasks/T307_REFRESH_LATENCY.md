<!-- SPDX-License-Identifier: GPL-3.0-only -->
# T307: Refresh parameter-dependent latency reporting (#307)

Independent L1 from #393; Keel's #307 go ruling is reporting-only (no
compensation, realtime DSP or ABI/protocol changes). This T2 task must receive
independent Claude/Keel review of the exact head.

## Read first
- `AGENTS.md`, `docs/planner/LEADS.md`, `docs/PLAN.md`, issue #307 Keel ruling.
- `native/engine2/engine2.h`, `engine2.cpp`, `tests/staged_tests.cpp`.
- `native/tests2/allocation_watch.h`; `native/sidecar/session.cpp` status cadence.
- `docs/SDK_AUTHOR_GUIDE.md` latency guidance.

## Implementation
- Reuse one allocation-free latency distance/bus calculation in topology and
  control-owner `Engine::refresh_latency()`; pre-size scratch at topology.
- Query every ABI2 module's `latency_frames()` on the control thread only.
- Refresh retained live instances after staged graph swap, before park ack.
- Refresh from the existing sidecar status tick (100ms; <=10Hz), never render.
- Probe dynamically published latency after rendering a parameter change, on
  multiple signal paths/buses and across retained-instance replan; zero
  allocations under `allocation_watch`. Preserve all existing assertions.
- SDK guide: atomic publication contract for process-vs-control reads.
- No new dependency, scheduler, field, protocol, module or device access.

Null-Pi runner is not requested for reporting-only scope; no physical Pi/audio
test. Do not conflate CI with subjective/hardware evidence.

```acceptance
tier: T2
budget: one independent builder session
size: <= 300 changed lines, <= 6 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R '^(abi2_staged_tests|abi2_live_replan_tests|abi2_engine_tests|abi2_sidecar_unit_tests|abi2_sidecar_protocol_tests)
  - ci: guard, linux-arm64, windows-x64, sanitizers green on exact PR head
stops: [ABI/protocol change, realtime safety design change, new dependency, test removal, >450 changed lines, protected CI edit]
```

The independent Claude/Keel T2 COMMENT review on the exact PR head is a
mandatory *human* merge gate, not a machine-executable acceptance command.

 --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64, sanitizers green on exact PR head
stops: [ABI/protocol change, realtime safety design change, new dependency, test removal, >450 changed lines, protected CI edit]
```

The independent Claude/Keel T2 COMMENT review on the exact PR head is a
mandatory *human* merge gate, not a machine-executable acceptance command.

