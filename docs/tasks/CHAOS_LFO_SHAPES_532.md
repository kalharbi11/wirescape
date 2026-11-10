<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS four-shape LFO addition (#532)

Owner-approved append-only modulation feature and exact-head gate for
`essentials.chaos_lite` (the stable internal module ID; user-facing name
**CHAOS**). Source implementation: PR #533. No ID migration.

Append slot 26 `lfo_shape` with 0 Sine (old default), 1 Triangle,
2 Sample & Hold, and 3 Smooth Random. Use a seeded, per-voice independent
RNG. Preserve the original oscillator/drift stream and old sine output.
Support reading v1 module state and saving/restoring v2 state.
Run exact-head native, web and sanitizer tests before review.
Do not rename or reorder original parameters. Keep the patch-guide section.

This task definition is registered separately to avoid artificially
increasing #533's strict eight-file implementation budget. Once merged
to main, PR #533 must integrate that main commit before acceptance runs.

```acceptance
tier: T1
size: <= 450 changed lines, <= 8 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - bash -lc 'cd apps/studio && npm ci && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green
stops: [ABI/protocol edit, existing slot/ID/range change, failure to load old state v1, non-realtime operation, new dependency]
```

Signed: ChatGPT (builder), 2026-11-10
