<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #444 Pi build preparation queue

Owner authorized Codex Lead to prepare Pi setup/testing concurrently, add
claimable tasks, and publish the testing handoff. Owner installation remains
after Windows testing. This planning task does not certify the app or hardware.

## SEND THESE — claimable briefs
1. Repo `kalharbi11/wirescape`, #445, branch `task/445-pi-preflight`.
   Read `docs/tasks/PI_SOURCE_PREFLIGHT.md` and latest issue handoff. Implement
   read-only prerequisite checks plus fail-fast source-build logs/provenance in
   the existing Pi updater. Run its exact local and executable Pi acceptance.
   Keep dirty-tree/ref guards; no automatic install or device access.
2. Repo `kalharbi11/wirescape`, #446, branch `task/446-pi-app-smoke`.
   Read `docs/tasks/PI_APP_SMOKE.md` and coordinate existing #386 test ownership.
   Verify isolated app/session/native settings and actual Electron/sidecar smoke
   on Pi ARM64 with null audio. Reuse the harness; report skips/missing display
   as incomplete. Run its exact local and executable Pi acceptance.

Both are T1, <=600 changed lines / <=8 files each, one independent-session COMMENT
review. Claim before editing; sign as the agent plus temporary role/date and
leave the AGENTS handoff. They can proceed independently; integrate on a common
accepted source SHA before declaring setup ready. No owners are preassigned.

## Current evidence and existing work
- Fresh Pi nightly dispatched: private run `42560014392`; pending at handoff.
  It resolves public main when it executes. Record actual source SHA from its
  report. No queue cancellation, concurrency change or duplicate dispatch.
- Dispatch-time main `50380fd371d3962d1d8497a33ee39247649bf7fb` passed hosted
  linux-arm64/windows-x64/sanitizers in public run `42556847769`.
- Local Pi script tests: 11 passed; concurrency test: 1 passed; private bridge
  tests: 27 passed. These 39 cloud tests are not physical Pi evidence.
- #101 prior nightly was `f51eccbfd6df1f997ad16012ee306679db176981`, with native
  build/tests/three offline 600-second soaks/hash checks passing. Baseline pending.
- #101 owns nightly/baseline follow-up; require measured provenance and review,
  never a cloud baseline. Native nightly does not build/test the Electron app.
- #386 owns expanded real-app cases; #274 owns physical-display contention;
  #296 owns 3600-second edit soak; #271/#45 own physical checks. Do not duplicate.
- #439/#443 and playable Groove/stable-16-step work govern the final Windows
  source choice. Preparation on today's main is not final release certification.

Compass: this is authorized independent source preparation, with no phase-goal
or packaging change. Keel: any new runner/CI/protocol requirement remains T2 and
must be separately authorized; the two builders may not invent those changes.
No Pi install, desktop entry, autostart, service, owner file migration, hardware
access, SSH or release is authorized by this queue.

## Planning acceptance only
Implementation acceptance lives in the two tasks above. Their future entrypoints
are deliberately not claimed as implemented by this docs-only planning PR.

Signed: Codex Lead (Codex), 2026-11-08 America/New_York.

```acceptance
tier: T0
size: <= 260 changed lines, <= 4 files
must_pass:
  - python -c "import sys; from pathlib import Path; sys.path.insert(0, 'tools'); from foundation.acceptance import parse, find_task; root=Path.cwd(); tasks=[find_task(root, n) for n in (444,445,446)]; assert all(parse(p.read_text())['must_pass'] for p in tasks); assert len(Path('docs/planner/STATE.md').read_text().splitlines()) < 60"
  - bash -lc 'git diff --check'
stops: [runtime change, workflow edit, dependency change, installation or hardware access]
```
