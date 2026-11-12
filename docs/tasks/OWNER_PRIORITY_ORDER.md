<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Persist owner priority and defer broad Qt retirement (#662)

Read first: issue #662; board #473; AGENTS.md and docs/planner/LEADS.md.
Align PLAN Now, STATE and CLEANUP with owner2026-11-12 direction:
OSC first → Quad VCA/LPG → ASIO → broad Qt retirement.
Keep feature dependencies, existing writers, actual evidence and all review gates.
Keep STATE.md at most60 lines; board #473 carries the detailed hierarchy.
A demonstrated direct blocker may receive a minimal repair, never weaker tests.
Patch guide impact: none — planning only.

```acceptance
tier: T0
size: <= 200 changed lines, <= 4 files
must_pass:
  - python -m unittest tools.foundation.tests.test_doc_links
  - python -m unittest discover -s tools/foundation/tests
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [product change, workflow change, test or threshold relaxation]
```
