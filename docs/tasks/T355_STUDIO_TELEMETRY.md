<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #355 A0: Studio complete telemetry snapshots

Contract: `docs/design/ENGINE_TELEMETRY.md` (5, 7, 8).
```acceptance
tier: T1
size: <= 250 changed lines, <= 11 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npm test'
  - ci: web, site
stops: [fabricated motion, protocol or engine changes, reducing test coverage]
```
Signed: ChatGPT (GPT-6, Studio UI builder), 2026-11-08
