<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS curated layout regression (#545)

**Problem:** the mouse/touch starter overlap check fails on three older
`CURATED_PATCHES` fixtures after CHAOS grew. The patch JSON in the public
library is a separate source and already passes its own layout test.

**Implementation:** widen the spacing to the right of each large CHAOS
module in the historical fixtures `hollow-chapel`, `breathing-walls`,
and `black-ice-choir`. Move downstream modules without changing any
audio parameters, cabling, cards, CSS, or overlap assertions.

**Review:** require exact-head browser evidence; check both mouse and touch
and compare patch audio/state before/after (unchanged by construction).

```acceptance
tier: T1
size: <= 150 changed lines, <= 3 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npx playwright test tests/starters.spec.ts'
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green
stops: [changing test thresholds, engine code, IDs, patch formats, unrelated layouts]
```

Signed: ChatGPT (builder), 2026-11-10
