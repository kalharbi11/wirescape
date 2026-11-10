<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

**Updated:** 2026-11-10, Keel (ASAP lanes + rulings). Owner failures preempt features.
Live heads and completed evidence govern; queued, stale and missing results never pass.

## Authority and queue
- Board #473 is the claim source. Codex Lead keeps it current and holds the owner's
  delegation to approve/merge after independent review and evidence (T2 still needs Claude).
- **Owner 2026-11-10: ASAP lanes, hold lifted for them:** Chord (#511→#512), MIDI (#500 Learn),
  OSC (#72 slices A→B→C), Groove (#505 G1 → #509/#510 G2 → #514 G4 chords). Other new starts still wait.
- Seven closeout PRs remain after486 merged1e20242; this planner refresh484 is one.
- Keel rulings 2026-11-10: #511 Chord last-wins for reused explicit IDs, budget 20 files;
  #500 Learn keeps one loadGraph but serializes edits while it's pending; #514 G4 pre-ruled.

## Merged 2026-11-09 (baseline `605da43`, compile repair `1e20242`)
- #463 Windows runtime, #400 latency, #442 real-app evidence, #474 PLAN D15–D19.
- Cleanup: R1 #476, R2 #478, R5b #479, R5a docs #477. W1 Windows setup #475.
- Sound/sequencers: #310 Interleave/Vocoder, #405 + #410 16-step, #458 Groove study,
  #451 Groove S1(a) native, #210 MIDI patchable controls (last_note, CC ranges).
- Pi chain: #450 preflight (Pi 28/28 at 1074cfa), #452 ARM64 Electron smoke (Pi pass e6df3d2).
- Compass closed 17 finished issues; finished task files moved to docs/tasks/done/.

## Owner evidence and the Pi session
- Windows first session on #463: WASAPI headphones, starters switch and sound,
  computer keyboard and Launchkey work at 48k/256. Shared WASAPI needs the device rate.
- Pi runner: Node 22.23.4 / npm 10.9.9 and Xvfb/xauth installed by the owner.
- #486 merged1e20242 after exact guard/Linux/Windows/sanitizers/task success;
  the combined MIDI/Groove fixture collision is repaired.
- **#483 / #453:** guides repaired; fixed main integrated at2b07cfc with identical
  authorized task483 (also in484); Closes483 and both Pi labels applied. When the
  docs-only exact head passes preflight + app smoke, pin its main parent as `AcceptedSha`
  in docs/PI_FRESH_INSTALL.md (Keel evidence-transfer ruling on #483), then merge.
- Then the owner session: PI_FRESH_INSTALL sections 1–6 + the Pi part of #271.

## Open PRs and rulings
- #399 device routes/rate refusal (T2): **Keel High**: `Pa_IsFormatSupported` runs in
  every warm poll enumeration; on ALSA the active hw device is busy, so the running
  output is marked unsupported. Scoped cooperator repairs probe classification/cache,
  warm polling and three Static findings at a86fba3; fixed main integrated8c340bb.
  Classifier/syntax local checks pass; exact hosted/Pi and Claude re-review remain.
- #461 Groove tests: fixed main integrated8c9b834; reviewed263-line/two-file delta
  retained; fresh required/task/Pi disposition remains.
- #480 refused draft: last Save/load Medium fixed and independently reviewed;
  fixed main integrated021cd63; delayed bridge/Electron proof awaits hosted gates.
- #482 R3a:e988e70 integrates fixed main and real Windows frozen-Qt comparison hook.
  Actual comparison remains pending; repeatability alone is not baseline equality. No hash
  rebaseline. R3b deletion and allow-test-removal require a
  test-to-replacement table; Codex Lead tags `archive/qt-final` (annotated) on
  R3b's merge parent, once.
- #481 R4 follows R3. R6 ABI1 #470 is Keel's, after R3.

## Housekeeping notes
- 2026-11-10 (Keel): tiers seniors Codex Lead + **Coda** (`Coda: T2 batch`), helpers (`lane/helper`);
  v3.1 doc trim + queue mode (LEADS.md). Deferred to cleanup: docs/planner/USAGE_DEFERRED.md.
- docs/tasks/README.md lists task files still cited by open work; move them to done/
  when those PRs merge.
- Permanent: required guard/Linux/Windows/sanitizers + task/Web/WASM/Pi as applicable.
  No force-push, direct main commits, device/SSH access, suppression or threshold weakening.
- #271 owner hardware, #274 display/performance and #296 actual 3600 s soak stay separate.
