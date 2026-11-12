<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-12, Codex Lead. Owner failures preempt features.
Board #473 newest SEND THESE is the live claim source; preserve existing writers.
Completed source-head evidence governs; running, missing and stale results never pass.
## Priority and authority
- PLAN Now: owner failures → 1.0 quick fixes/release → OSC and Quad → 1.1 queue.
- 1.0 still needs #599/#607 and owner MIDI/release evidence; #593 owns the tag.
- Seniors may approve T0/T1; opposite-family T2 review remains mandatory.
- Keel retains ABI/SDK/spec/CI-policy approval; no protected approval inferred.
- No new feature starts while the implementation queue exceeds PLAN D18's limit.

## Immediate CI recovery and lead decisions
- #635/#638 at e0db32d: runtime-only FFmpeg provisioning, 1200s command/30min job,
  bounded apt retries. Keel cross-family/protected review requested; CI running.
- After #638 merges, builders merge main normally to use its workflow; plain
  reruns retain the old merge ref. Never weaken assertions or skip tests.
- #596/#617 at acb2adf: CI/Static green, Codex cross-family COMMENT posted.
  Medium: blanket docs/ exemption skips native tests for a consumed JSON fixture.
  Keel narrows classifier, adds fixture-only proof, integrates #638; new review needed.
- #637/#639 at f4121bc: hosted-Windows wake-only single retry, strict limits,
  negative subprocess cases; Coda T2 requested. CI running, no pass claimed.
- #605/#608 at c80c7ea: Windows red on pinned Ninja vendor suffix
  1.13.0.git.kitware.jobserver-pipe-1. Keel ruling pending; broad ranges excluded.
- #601/#611 at 70c7b67: fresh Q1 CI running. Four-channel/eight-voice cost probe
  and catalogue repairs required; explicit size disposition remains with leads.
  Keel accepts ordinary module CI CTests without extra Pi evidence. Q2 #602 waits.

## Existing builder continuations
- OSC C1 #603/#618 at1517639: Coda T1 approval exists, but conflicts and accept603
  600s timeout remain. Log confirms repeated native build, before Playwright.
  Integrate #638/main, resolve conflicts, full acceptance and changed-head review.
- OSC C2 #604/#620 ate40aaf2: BLOCKED on Coda High: raw or split UTF8 OSC
  address can throw at telemetry JSON dump. Printable ASCII capture + loopback
  regressions required; then main integration, Coda T2 and Keel spec approval.
- OSC row enable #612 follows actual C1 merge; #94 follows full J5 per task.
- Groove #614/#619 at499afc0: integrate main/task, format DSP/observation files,
  prove frozen unpatched events and exact note-offs at two external intervals.
- Groove browser #625/#626 at7d83dce: Web demo engine/site SUCCESS, 22 tests,
  including audible patch/mobile offscreen cable drag. Acceptance/sanitizers/
  Static still incomplete; integrate #638 and obtain independent final T1.
- Studio #599/#607: existing writer fixes review findings and integrates #638;
  full acceptance and source-matched required/Web evidence remain mandatory.
- ASIO #627/#631 atc2c36dc: Keel buffer ruling already issued: p=1..4096,
  quiescent prepare, one retry, actual-size status, no callback resize/persistence.
  Integrate merged #630/main, preserve attempted error through rollback refusal;
  fake regressions, Coda T2 and Keel protected-spec final approval remain.

## Deferred work and evidence boundaries
- #482 R3a / R3b / #481 R4 parked until owner #581 test per Keel; no feature gate.
  Preserve frozen replacement evidence; R4 follows full R3b and archive/qt-final.
- #487 retains D5 scope, separate from merged #572 wake scheduler; signed head
  handoff requested. #560 CPU table blocked on dependency fetch; no invented data.
- #91 requires Keel guard authorization and trusted baseline collection route.
- #271 hardware, #274 physical display, #296 actual3600s soak remain distinct.
  AcceptedSha cd7999e4a02308df0db918cfbab6974b97b6782f certifies only its evidence.
- Dropped #554/#565 branch at73bfe95 is preserved; annotated archive tag pending.
- Permanent: exact required/task/Web/Pi as applicable; no direct main, force push,
  devices/SSH, fabricated evidence or weakened thresholds.
