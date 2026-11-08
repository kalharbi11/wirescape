<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #355 A0: Studio telemetry complete snapshots

**Issue:** #355. **Tier:** T1. **Contract:** `docs/design/ENGINE_TELEMETRY.md` rulings 5, 7, 8.
**Shared brief:** `docs/tasks/T350_ENGINE_TELEMETRY.md`.

Replace all four live maps on every Studio tick; absent means unavailable.
FakeEngine reports all cable-modulated parameters each tick, including amount 0,
and clears old event activity on graph replacement/removal.
Ignore older graph generations; controls revert to base without a sample.
No protocol, engine, SDK, or audio changes.

```acceptance
tier: T1
size: <= 250 changed lines
must_pass:
  - cd apps/studio && npm ci && npm test
  - Playwright: disconnect clears knob and filter within one tick; older-gen ignored
  - ci: web, site
stops: [fabricated motion, protocol or engine changes, reducing test coverage]
```

Signed: ChatGPT (GPT-6, Studio UI builder), 2026-11-08
