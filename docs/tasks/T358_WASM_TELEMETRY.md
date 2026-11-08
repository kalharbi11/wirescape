<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #358 A3: wasm telemetry from engine2 taps

**Contract:** `docs/design/ENGINE_TELEMETRY.md`, issue #358.
Requires merged A1 #362 and wasm cadence cap #353. The candidate
tap planner and snapshot sampler are shared with sidecar A2 #357,
on which this implementation PR is stacked.

Install taps before candidate prepare, read only render-owned samples
on the single AudioWorklet thread after `wsw_render`, and return complete
`effective`, `phase`, `envelope` and `eventActivity` maps. Preserve
existing `outputs`, `scope`, `meters`, and `heldNotes`. Phase remains
empty pending B-wire #359. Reset event-count cursors and activity decay
after every topology commit; test the first next burst. The worklet frame
cap remains ≤30 Hz and is not changed here.

**Read first:** `native/web/web_engine.cpp`,
`native/web/web_engine_tests.cpp`, `apps/studio/src/web/ws-processor.js`.

**Further gates:** native web test; wasm build; Playwright web site;
web and web-demo CI; one independent T1 review; report telemetry JSON cost.

```acceptance
tier: T1
size: <= 300 changed lines, <= 3 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
stops: [telemetry above 30 Hz, fabricated values, engine2 threading change]
```

Signed: ChatGPT (GPT-6, web telemetry builder), 2026-11-07 (America/New_York).
