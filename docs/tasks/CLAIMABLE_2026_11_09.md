<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #393 Claimable integration queue — 2026-11-09

Owner priorities: Groove S1 native first; stable16-step baseline; Interleave before Vocoder; MIDI and
OSC actively progress. Groove already has agents: do not duplicate that lane.
Lead review checkpoint #455; excludes #439 and Pi install/setup.
This replaces stale ready/unclaimed lists. The live issue's latest claim wins.
A handoff-ready branch can be continued; check for newer work before editing.
Claim on the issue, continue its existing branch, sign exact-head evidence.
One issue/branch/PR per item, <=3active items per agent. No device/SSH/LAN tests,
test/threshold weakening, history rewrite or fabricated queued results.

## READY 1 — Interleave first (#305 / #310)
Repo kalharbi11/wirescape; branch task/305-spectral-b, current83a4b05.
Read305/310handoff and docs/tasks/S1_1B_SPECTRAL.md on that branch.
Merged304 is not a blocker. PR already targets main; integrate without force-push.
Prioritize Interleave mode6: A/B chunk timing including silent frames, source vs
phase-vocoder mode, continuity, pitch/stretch, reset, statev1/v2 and transitions.
Preserve shipped0–5modes, approved parameter slots, ASan partial-final-block fix
and hop-boundary stress assertions. If Vocoder delays release, split a bounded
Interleave-first delivery with stable IDs/slots; don't expose an unimplemented
mode or skip existing assertions. A combined PR must still pass full acceptance.
Native/Studio/WASM + task-specific Pi stress, independent T1, patch-guide update.
Run python tools/dev.py accept305 plus actual must_pass (including executable
Pi cases); queued/prior-head checks are not proof. Done: playable reviewed mode6,
correct current catalogue, source/phase behavior and persistence proven.
Vocoder polish follows; no speculative new spectral architecture.

## READY 2 — MIDI completion (#381/#390; #290/#210; then #287)
Continue task/381-midi-learn and task/163-midi-patchable, not duplicate APIs.
Read latest381/390and290/210 handoffs and their named tasks.
390current6a8be72 is mergeable and hosted/static/Web/WASM green; no stale conflict task.
210currentdf81486 has exact Linux/Windows/sanitizers/privatePi PASS and independent
T1 inspection without correctness findings; authored formatting and803cppcheck
introduced-vs-upstream diagnostic classification remain, not an inherited-red waiver.
Task13file allowance already repaired. Keel approved base6fdd4d1
semantics, but helper refactor/new semantic deltas need his final cross-family look.
Main reconciliation and fake-device/native/mock-bridge checks can proceed NOW.
Prove latest-wins CC capture, rearm/expiry/source removal/restart, MIDI1/UMP,
lock-free/no-allocation ingress and unchanged wire/config/unknown data.
210controls are independent: last_note, CC ranges/inversion/smoothing/configv1→v2,
four/eight-note allocation, sustain, steal, disconnect/choke/Panic and no stuckoffs.
After381actually merges, implement287 Learn UI → persisted cc_to_cv rows perD8.
Run accept381/290/287 and each task's native/Studio/Pi lines on exact heads.
Done: reviewed native seam + persisted UI mapping; no claim that browser supports
hardware MIDI. Chord's NOTE events need a separate production allocator, not midi.to_cv.

## READY 3 — Current-host OSC (#72 / old #82)
Continue reusable task/72-osc work only after checking its latest handoff.
Old82reviews cover oldQt/CLI code, not current Electron/stdio sidecar support.
Read docs/tasks/J5_OSC_MONITOR.md, current architecture/protocol and82code.
T0 inventory #440/#441ea39c09 already exists; finish review/gates for
docs/tasks/OSC_CURRENT_HOST_72.md: reusable codec/service/module/tests vs missing
sidecar/Studio/Monitor/launcher seams. Keep frozenQt unchanged and avoid copying
the entire stale52file PR into current app. Existing osc.in/monitor IDs stay.
Then implement approved portable/current-host slices, with a separate Keel T2
decision before any new protocol/settings/engine-ownership contract.
Defaultlocalhost, LAN disabled and explicit owner consent; bound rings/strings,
sharedports/lifetimes, stale timeout, reset/churn and scheduling must be tested.
Tests may use localhost only. No realLAN/device opening; browser network feature
remains unavailable unless independently authorized. No OSCOut/scope expansion.
Run original offline OSC/Monitor/lifetime tests and current-host acceptance;
new task must replace old non-executable prose with actual executable commands.
Done: current native app can patch supported OSC input and monitor it, independently
reviewed T2/current-head evidence. Inventory-only is not implementation completion.

## READY 4 — Evidence and soak (#385/#398 -> #386; #296)
#398 is MERGED; do not repeat its task-only repair or native implementation.
Read latest actual checks; source780e280 had all5render CTests and task-specific
Pi acceptance pass; lead independent T1 review posted, size553→560 synchronized.
Keep merged provenance/thresholds. Continue #386/#442 real-sidecar E2E
save/reopen/refusal/reconnect/numeric telemetry; no second harness.
Then386extends existing Electron real-sidecar e2e, not a second harness.
296private lane remains open: fixed ci/pi-edit-soak/3900s timeout, independent
Claude T2 review and actual3600s fake-device source-head evidence. Inspect private
policy and newest claim before a bridge PR. No generic timeout changes.
387owner checklist reduction waits385/386/296evidence; no hardware waiver.

## READY 5 — Latency repair (#307/#400)
Continue task/307-runtime-latency after latest owner/handoff check.
Keel's existing go ruling permits work. Actual static log on6dcc370 has79+233
authored formatting diagnostics; cancellation is NOT an infrastructure waiver.
Format only changed engine2.cpp/staged_tests.cpp code with pinned formatter,
classify other diagnostics accurately, reconcile main and rerun focused5CTests
and required/sanitizer/static checks. No suppression/threshold/ABI changes.
The control-thread latency behavior remains T2; obtain Claude final review.
Done: parameter-dependent latency refresh validated without callback allocation.

## READY 6 — Patch publishing rollout (#426/#427)
Existing task/426-patch-designer-library. Current1207e40 selected acceptance/required jobs/static/Web/WASM and current-head
independent review verified by lead; merge held for explicit owner permission after
auto-review rejected enabling auto-merge. Do not claim merged/deployed.
Runtime infrastructure has independent
T1 review with Medium findings repaired; source variants passed31focused UI tests
and WASM site, but final exact-head/full evidence and main reconciliation remain.
Continue current branch, preserve latest guide/AGENTS owner delegation and STATE;
don't overwrite live sequencer docs or module catalogues. Inspect latest427head.
Resolve selected task-command bootstrap (bare Node was wrapped in bash) and actual
new failures; don't misclassify shell setup as product correctness.
Run accept426 and full Studio/Web-demo/current required checks, then authorized
gate + verify main deploy/live and public upload/update/delete behavior.
Future JSON-only designers have standing owner self-review/merge authority.
No GitHub credentials/live sync in this implementation. Future428stays separate.

## Not a new claimant / blocked contract
- Groove432 native451: child454, task linkagebaceee6, lead index fix31e0aa8.
  Existing builder: actual mute/conformance rerun, saturated NOTE_OFF/reload-pending
  identity tests, authored formatting, accept454/Windows/privatePi/T1. No UI changes.
  S1(b) waits designer + Keel STOP/playhead observation ruling, not browser inference.
- 401/405and408/410are with existing sequencer agents; explicit regression gate
  is in docs/sequencer/README.md. Groove430also stays with its existing agents.
- Quantizer323is already implemented and independently reviewed at0acdad4;
  native/Windows/sanitizers/Studio/WASM/task PASS there. Root corrected2format-only
  count-assertion wraps at385ad20. Exact385ad20 acceptance/required/Web/WASM green; static red and current-head delta
  review remain. SDK64 #420 MERGED84f926b via gate after signed approval;
  selected commands/required/static reviewed green. No duplicate DSP.
- Chord406 + Notes→CV423: Keel source/note identity/capacity/expression/channel
  ruling still required.423Medium baseline repair is executable docs work:
  directMIDI baseline native only, supported non-midi note source for WASM.
- Audio routes399: fix unsupported task command via supported bash/Chromium,
  then fake-state regression; native mono/backend/stream-restart needs Keel.
- New sound.cross_mod332is lower priority until Interleave/MIDI/OSC slots clear.

Signed: Codex Lead (Codex), 2026-11-09

```acceptance
tier: T0
size: <= 300 changed lines, <= 3 files
must_pass:
  - python -c "from pathlib import Path; assert len(Path('docs/planner/STATE.md').read_text().splitlines()) < 60; t=Path('docs/tasks/CLAIMABLE_2026_11_09.md').read_text(); assert sum(l.startswith('## READY ') for l in t.splitlines()) == 6; assert '16-step regression gate' in Path('docs/sequencer/README.md').read_text()"
stops: [runtime or protocol change, threshold weakening]
```
