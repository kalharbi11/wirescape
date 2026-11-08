<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #359 B-wire: real LFO phase in Studio and telemetry hosts

**Contract:** `docs/design/ENGINE_TELEMETRY.md` ruling 12, issue #359
and `docs/tasks/T350_ENGINE_TELEMETRY.md`. B-mod #364 and A2 #368
are merged. A3 web host #369 must merge before its phase mapping is wired.

Enable the existing host-side `essentials.lfo:phase` output tap (first
channel, rendered last frame) and publish finite measured phase in [0,1).
On Studio's WavePreview, show the dot only for a received phase sample;
never interpolate, wrap malformed phase or invent samples. Empty and
stale-generation snapshots must clear the dot. Keep the engine's ≤30 Hz
cadence and all existing telemetry fields unchanged.

**Read first:** `native/document/telemetry_taps.h`,
`native/sidecar/session.cpp`, `native/sidecar/protocol_tests.py`,
`native/web/web_engine.cpp`, `native/web/web_engine_tests.cpp`,
`apps/studio/src/panels/widgets/WavePreview.ts`,
`apps/studio/tests/revision.spec.ts`, `apps/studio/AGENTS.md`.

**Further gates:** native null-sink LFO phase advancing in [0,1);
native wasm host regression after #369 merges; Playwright dot presence,
unavailability and no interpolation; engine, web and web-demo CI;
independent T1 review. No device runs.

```acceptance
tier: T1
size: <= 200 changed lines, <= 8 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
stops: [interpolated or decorative phase, telemetry above 30 Hz, DSP or SDK/ABI change, protocol change]
```

Signed: ChatGPT (GPT-6, engine/UI telemetry implementation builder), 2026-11-08
