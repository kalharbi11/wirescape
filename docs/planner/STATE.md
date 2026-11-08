<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-08 by Codex Lead; retains Compass/Keel rulings.
Pi first. Owner-reported #271 failures preempt all work; owner sign-off remains separate.
Read current issue handoffs/heads before acting; this is a queue, not a claim of acceptance.

## Authority and operating rules
- Codex Lead approves T0/T1, writes named docs/tasks/** and STATE paths, applies exact-head
  Claude conditional approvals, and merges stuck reviewed green heads under AGENTS delegation.
- T2 Codex changes need Claude cross-family COMMENT review; batch real decisions for Keel.
- No devices/SSH/LAN, force-push, fabricated measurements or loosened thresholds.
- Required guard/linux-arm64/windows-x64/sanitizers plus task-specific evidence must pass.
- Static/cortex inherited failures require classification; no blanket authored-failure waiver.
- Gates/runners wait. Do not continuously poll. Sign and hand off exact SHA/tests/artifacts.

## Four owner-assigned lanes (current source of truth: #383)
- Full briefs/dependencies: #383 comment 6720886151; latest follow-ups supersede snapshots.
- Agent1: #378 callback gate/evidence; #296 public PR #389 (child acceptance #391), then private
  ci/pi-edit-soak after public merge; finally #384 private wording/baseline. Sole private bridge editor.
- Agent2: #369 final checks/merge -> #379 both-host phase; existing module finishers while blocked.
- Agent3: #381 PR #390 transient CC seam -> #287 UI after merge; #210/#290 fallback.
- Agent4 (serial): #384 public PR #388 -> #385 renderer -> #386 app integration after renderer merge.
- #388 has independent T1 review and 2+23 local focused tests passing; hosted checks still required.
- #385 case3 sound.blend confirmed; no private runner change or ci/pi-audio label.
- #387 owner-checklist reduction waits for verified #385/#386/#296. Real capture remains deferred.

## Current integration and review
- #373 poly probe and #346 oscillator UI merged. #301 recovery/#368 telemetry/#371 rename merged.
- #369 has independent T1 review; exact-head Windows requested. Keel allows ONLY the known
  native/web compiled-owner static gap; #369 does not wait for #380.
- #382/#380 implementation 27 local tooling tests pass; missing taskfile fixed at5766534.
  Needs Claude T2 review and fresh final-head checks; verify native/web coverage/timing artifact.
- #378 numerical median-of-five <=1.05 gate fixed at501a894; final CI/Studio/pi5/accept required.
  Keel conditional disposition applies only when all named evidence exists on that exact head.
- #389 child #391 routes public smoke acceptance; parent #296 stays open for private/hour evidence.
  Agent1 correcting invalid size syntax/selector; strict bench parsing includes malformed records.
- #390 needs Claude T2 review; #287 waits for its merge. No Pi needed per control-path ruling.

## Modules and free-agent queue
- Agent2 serializes #304 integration/Pi -> #323 catalogue and #310 after #304 merge.
- #344 can use merged #373; two instances x eight channels Pi acceptance required.
- #345 download failures need scoped triage; #348 actual sidecar protocol Linux failure must be fixed.
- FREE/UNCLAIMED: #293 routes, inherited dependency now merged; claim before work.
  Full scoped brief + named task path in #293. New protocol remains a Keel stop.
- #361 diagnostic attribution needs protected CI authorization; no exclusion/suppression shortcut.
- #82/Phase3 and new modules remain parked behind current priorities.

## Owner-requested checkpoint cadence
- Four builders return at +3h/+6h if unfinished; Claude reviews +2h/+6h;
  other independent reviewer +2.5h/+4h. Lead checkpoints +1h/+3.5h/+7h.
- Lead reviews outputs, unblocks/merges, refreshes return-task handoffs and leaves unclaimed briefs
  for the free agent. These are owner-requested finite checkpoints, not a new monitor framework.
