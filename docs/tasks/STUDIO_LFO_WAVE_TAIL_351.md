<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio LFO waveform: no moving tail without phase (#351)

## Purpose
Separate true `outputs[moduleId:control]` amplitude samples from the illustrative two-cycle LFO trace. The trace must move only when the engine supplies actual `phase[moduleId]`. Today a changing output sample is grafted onto the last pixel of a frozen reference curve and looks like a tiny false animation.

## Read first
- `apps/studio/AGENTS.md`, `apps/studio/docs/DESIGN.md` §6, `apps/studio/docs/PERFORMANCE.md`
- `apps/studio/src/panels/widgets/WavePreview.ts` and `waveTrace.ts`
- `apps/studio/tests/p1g-ui.spec.ts`

## Work
- Without phase telemetry, draw a stable reference shape while visibly saying "phase not available"; changing output samples must **not** change its pixels.
- With genuine phase telemetry, scroll the two-cycle trace; a separate numeric amplitude readout may continue to use the latest observed output value.
- Keep current colors, dimensions, one ticker loop, redraw-on-change optimization, no timers and no decorative oscillator.
- Test both missing phase + changing output and present phase + phase changes using the injected ticker. Do not assume FakeEngine is evidence of real engine phase support.
- Document which module telemetry is host-observable and what needs Keel's ruling in `docs/reviews/ENGINE_TELEMETRY_MODULE_PREFLIGHT_2026-11-07.md`; #350 owns engine implementation.

```acceptance
tier: T1
size: <= 200 changed lines, <= 4 files
must_pass:
  - ci: Studio web job build and Playwright LFO regression on exact PR head
  - ci: remaining Studio Playwright tests
  - review: independent-session T1 review before merge
stops: [ABI edit, protocol or SDK change, fabricated signal motion, new dependencies, weakening tests]
```

Signed: ChatGPT (GPT-6, UI builder), 2026-11-07
