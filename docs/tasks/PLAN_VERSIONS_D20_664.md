<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Plan D20: app versions are releases, not development phases (#664)

Read first: issue #664; `docs/PLAN.md` Decisions; `docs/releases/v1.0.md`.

Record the owner's versioning decision (2026-11-12) as PLAN D20, drop "Preview 0.7" and "Beta 0.8" from the phase headings, and update the 1.0 release notes (status "in development", scope = 1.0 + #597 fixes + OSC, Quad VCA/LPG, ASIO/FlexASIO, #599). Same PR: refresh PLAN "Now" to the owner order OSC → Quad VCA/LPG → ASIO (supersedes #663's PLAN hunk), the CLEANUP "Status and order" note, and STATE. Planning files only.

Patch guide impact: none — planning docs only.

```acceptance
tier: T0
size: <= 120 changed lines, <= 6 files
must_pass:
  - python -m unittest tools.foundation.tests.test_doc_links
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [product change, workflow change]
```
