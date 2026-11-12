<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC forward-compatible config and row keys (#657)

Follow-up to Keel's #645 ruling: future-authored `osc.in` configuration
must not refuse an otherwise valid patch merely because this engine does not
understand a key. The original patch JSON stays owned by Studio.

Read first: `native/document/runtime_module_config.h` (OSC adapter),
`native/document/runtime_patch.cpp`, `native/document/runtime_module_config_tests.cpp`,
`apps/studio/src/app/OscEditor.ts` and its Playwright tests.

## Implementation
- Ignore unknown keys in `osc.in.config` and in each `rows[i]` only;
  preserve all existing validation and refusal for **known** keys and values.
- At most once on successful graph load, print a bounded list of ignored paths
  in control-side diagnostics, not on the realtime audio thread.
- Studio says “ignored by this engine” while preserving unknown authored
  data until an explicit editor Save. No protocol fields or SDK/ABI change.
- Add native acceptance/refusal and Playwright message tests; update the patch
  designer guide with actual engine behavior.
- Coordinate with #645's separate existing per-row `enabled` implementation;
  do not edit that branch or silently treat other module config types as OSC.

```acceptance
tier: T2
size: <= 320 changed lines, <= 10 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [ABI or protocol format change, known-key validation relaxation, test removal, render-path logging, device or LAN access]
```

Signed: ChatGPT (GPT-6 builder), 2026-11-11 EST.
