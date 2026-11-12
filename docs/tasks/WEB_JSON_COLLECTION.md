<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Restore Node JSON fixture collection (#644)

Read first: AGENTS.md; issue #644; apps/studio/tests/curated.spec.ts import pattern.
Add JSON import attributes to the two Web module-matrix fixture imports only.
Keep fixtures, product code, browser assertions and test selection unchanged.
Patch guide impact: none — test loading only.

```acceptance
tier: T0
size: <= 40 changed lines, <= 2 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright test -c playwright.web.config.ts --list'
  - ci: web-demo engine/site and guard, linux-arm64, windows-x64, sanitizers green
stops: [product change, test removal, fixture modification, workflow change]
```
