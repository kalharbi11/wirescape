<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-08 agent-review checkpoint, Codex Lead owner-requested audit.
Pi first; #271 failures preempt features. Owner Phase1 sign-off remains open.
Delay #345 and runbook #422 now merged; live heads/latest handoffs supersede older briefs.

## Authority and evidence
- Codex Lead T0/T1, named docs/tasks/** and STATE paths, delegated reviewed-green merges.
- T2 requires independent cross-family review; no device/SSH/LAN, force-push or threshold weakening.
- Required guard/Linux/Windows/sanitizers plus task evidence; queued/stale/missing is not passed.
- Preserve IDs/unknown patch data, sole Electron automatic restart ownership. Do not poll CI.
- #407 gate-v4 MERGED ec1eea5: signed neutral exact-head approvals now available.
- Existing #419 STATE PR is older; reconcile it with this checkpoint before merging.

## Verified merged and app availability
- #301 audio recovery; #368/#369 native/WASM telemetry; #378 callback CPU/cumulative dropouts.
- #343 Macro gate, #346 musical VCO tuning, #364 LFO phase output (display wiring pending).
- #344 Resonator, #304 Spectral base modes, earlier #329 Blend; Delay adds the25th module.
- #331 patch browser; earlier autosave/recovery/hotplug/keyboards/live edits remain merged.
- #373 poly probe, #382 static ownership, #388 evidence syntax, #389 public edit soak,
  #396 dangling-reference tests, #397 diagnostic fixtures, #404 param64, #417 Qt slots fix merged.
- Shared native catalogue covers Electron and WASM; browser intentionally omits3 midi.* modules.
- Web deployment42509218198 atb4ec649: engine/site/deploy/live all SUCCESS.
  This verifies that deployment, not latest-main/browser hardware or future module branches.
- tools/pi/update-and-build.sh resolves one SHA and rebuilds engine + Studio + Electron.
- Refreshed owner runbook: docs/tasks/OWNER_TEST_SESSION_271.md; #271 body updated.
  All24 modules, 3/4-note MIDI, persistence/recovery, CC matrix, web, original600s benchmark.

## Active integration queue (continue owners/branches; no duplicate claims)
- #345 Delay MERGED; #348 Reverb conflicts again. Preserve upstream registrations, regenerate
  catalogues with Python, normal main merges, fresh exact-head gates/Pi and narrow T1 review.
- #323 Quantizer conflicts too; #304 is merged, no longer a blocker. #310 additional modes open.
- #379 LFO phase wiring conflicts; integrate #417, actual compiled static ownership required
  and Keel strict-static disposition retained. No fake/interpolated dot.
- #390 Learn seam conflicts; task13-file allowance stands; callback helper needs Claude look.
  #287 production UI waits for seam merge; #210 patchable controls remains open.
- #399 routes: owned, native mono/backend/stream-restart contract needs Keel.
- #400 latency: owned, final static + Claude T2 review; cancellation is not a waiver.
- #415 selector: owned, final revised tests and Keel review. #420 SDK64 regression is open.
- #405 native16-step -> #410 editor now unblocked by actual #404 merge; keep order, review,
  old8-step compatibility, exact-head tests. #412 independent T1 review passed atff8a95b.

## Chords and next phase
- #406 design corrected at a2a73c5; product IDs/eight controls already confirmed.
- Owner accepts3/4-note first; fifth optional. Existing MIDI→CV defaults4 and supports1–16.
- NEW #421 tracks missing production Notes→CV allocation route; do not duplicate #406.
  Keel must resolve fan-in identity/anonymous offs/expression/capacity and adapter route first.
- #296 still needs private fixed ci/pi-edit-soak lane/3900s timeout and actual3600s evidence.
- #398 offline audio evidence -> #386 real-sidecar e2e -> #387 checklist reduction remain.
  No owner gate removed before evidence. #82 OSC/Windows hardware/packaging stay later.
- #412 owner-requested product/code approval posted; Windows requested, auto-merge enabled.
  All8 shared Studio/WASM/Electron starters validated; published deployment follows merge.
  One issue/branch/PR per item, max3 active per chat, separate review and signed handoff.
