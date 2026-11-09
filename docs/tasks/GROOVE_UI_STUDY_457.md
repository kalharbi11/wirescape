<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #457: Groove S1 offline UI study closeout

Reference-only child of #432; governing brief: docs/sequencer/UI_DESIGN_BRIEF.md.
Deliver the HTML/CSS study and design notes; preserve the acknowledged loaded
snapshot and refusal across disconnect/recovery. Start requires a matching Apply
ACK for changed drafts. No production editor, engine, protocol, native telemetry,
dependency, patch format or device changes. Parent #432 stays open.

Read first: root and Studio AGENTS, the brief, Groove plan and Studio UI guidance.
Real Chromium checks cover dirty/refused recovery, queued STOP locks, console,
focus and 1280x720/1920x1080/390px layout. Screenshots are workspace artifacts,
never committed; independent source/visual T1 review and exact-head CI precede merge.

```acceptance
tier: T1
size: <= 900 changed lines, <= 6 files
must_pass:
  - npm --prefix apps/studio ci --no-audit --no-fund
  - node --test apps/studio/tests-node/groove-study.test.mjs
  - python -m unittest discover -s tools/foundation/tests -p test_doc_links.py
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
stops: [production source or native change, new dependency, inferred native STOP or playhead, author approval or merge]
```

Signed: Codex Lead (Codex, delegated Groove study closeout), 2026-11-09 America/New_York.
