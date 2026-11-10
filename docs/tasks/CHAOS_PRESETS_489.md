<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS-PRESETS: Studio CHAOS-lite presets (#489)

Implement the issue's eight presets as Studio data/UI. Preserve unknown patch data and cables; use existing history and setParam behavior. The optional offline WAV Playwright case must use the production renderer and unchanged #398 evidence metrics.

```acceptance
tier: T1
size: <= 900 changed lines, <= 12 files (Codex Lead authorization, 2026-11-10)
must_pass:
  - bash -lc "python tools/dev.py build --headless --config Release"
  - bash -lc "cd apps/studio && npm ci && npm test"
  - bash -lc "cd apps/studio && CHAOS_RENDER=../../build/native/wirescape_render npx playwright test chaos-presets.spec.ts --grep 'offline WAV'"
  - ci: guard, linux-arm64, windows-x64, sanitizers and web green on the exact head
stops: [native/protocol/catalogue format change, parameter ID change, preset that alters connections]
```
