<!-- SPDX-License-Identifier: GPL-3.0-only -->
# v1.0 notes: plans, research and brainstorming (owner + Keel)

**What this file is.** A running list of everything the owner and Keel discuss for v1.0: fixes from
the owner's test sessions, plans, research and open decisions. **Nothing here is a task.** Agents
don't build from this file and the board doesn't read it.

**How it's used.**
1. Anything we discuss or put aside for v1.0 gets a section here (plan, research, open decisions).
2. When the owner says "let's work on X", Keel reads its section, re-checks it against current
   `main`, says what the plan was, and offers fixes or updates to it.
3. Only after the owner approves does it become issues and task files on the board.
4. Keel keeps each section's **Status** line current. Done items move to "Done" at the bottom.

Status words: **idea** (not planned) · **planned** (plan below, not approved) · **approved** (in tasks) ·
**in progress** · **done**.

---

## A. In v1.0 (owner-confirmed)

### A1. OSC Out to a local program (owner 2026-11-13, PLAN D21)
**Status:** in progress. Module #736, sender #739, Studio card #738 and guide/receiver/example #740
are merged. Left: Pi soak evidence O5 #734 (Codex Lead), phantom gate-rise fix #759.
Contract: `docs/specs/OSC_OUT.md`. Loopback only. LAN destinations and OSC bundles with timestamps
are later (see B).

### A2. Live editing without restarting the patch (owner 2026-11-14: in v1.0, from session feedback)
**Status:** planned (not approved).
**Problem today.**
- Parameters are live (events). Adding or removing modules and cables is a live replan: kept modules
  keep their state.
- Any module-config edit, undo/redo or snapshot restore sends a **full `loadGraph`**: mute, fade,
  and every module restarts (phase, delay and reverb tails, envelopes, held notes, sequencer position).
- **Groove is worst.** Steps, length and **tempo** are config. Edits are a draft, Apply works only
  while stopped (`RealEngine.canApplyGroove`), and the whole patch reloads, so play starts from step 1.
- Also hit: OSC In row edits, MIDI CC Learn and undo/redo (all full reload). OSC Out is rebuilt on
  every edit (harmless re-send). Voice-count/mode changes on MIDI to CV, Host Keyboard and Notes to CV
  release held notes (expected; no warning shown).
**Plan.**
- L1 (Keel contract): a `setModuleConfig` protocol command. A module may declare that it accepts live
  config. The engine sends it the new config as a `WS2_MESSAGE` (the ABI already carries data and the
  engine has a message arena, so no ABI change), the module swaps at a block boundary keeping its
  state, and the engine records the new config bytes so later replans still retain the module.
  Other modules get a per-module rebuild (only that module restarts).
- L2 (T2): the engine/sidecar path and device-off tests: edit while playing, other modules keep their
  phase, no allocation in `process`.
- L3 (T2): Groove live pattern. A step edit sounds the next time that step plays; tempo changes
  keep the phase; a length change lands at the end of the cycle; the playhead never resets.
- L4 (T1): Studio. Groove drafts go away (edits stream live, debounced about 30 ms, normal undo).
  OSC In, CC Learn and undo/redo use the new command. `loadGraph` stays only for opening a file.
- L5 (T1): a warning before a voice change releases notes, OSC Out not rebuilt on unrelated
  edits, and a Pi soak of live edits while Groove plays.
**Open decisions (owner).** Groove step edits: immediate (Keel's default) or at the next bar?
Order relative to the Windows fixes in A3?

### A3. Owner Windows test findings (2026-11-13)
**Status:** in progress.
- ASIO list frozen while playing + safe device switch: #722 / PR #758 (gpt-builder3; Keel review
  posted, needs one spec line).
- Studio lists installed-but-failed ASIO drivers: done (#726).
- MIDI to CV pitch/gate not reaching VCO/ADSR: #724 / PR #727. The engine path tests fine; this
  needs the owner's patch file and whether the missing pitch is audible.
- FlexASIO not listed: owner evidence steps are on #581 (registry list, FlexASIO.log).

### A4. OSC In scaling and ESP32 controller
**Status:** done (#742/#744, #746/#748). The owner test on the Pi is pending. Follow-up: a
LAN-only launch warning (#760).

### A5. First owner test session findings (#597, 2026-11-11)
**Status:** partly done; Keel to re-verify each item against `main` before work.
- Done: CHAOS fall sticking, reverb Mix level, ASIO build (#598); Random Source scope, unplug from
  either end, cable activity (#599); Groove clock/run/reset inputs (#614).
- Not verified as fixed: basics-04 silence after short envelope + high VCA; scene-07 clicks after
  panic; Echo knob clicks; Groove no pitch/gate into VCO/ADSR (item 9, maybe the same as A3's MIDI to
  CV); Groove start/stop lag (item 10: show "queued" or start immediately when stopped).

### A6. Release 1.0.0
**Status:** waiting. New candidate SHA after A1 O5 passes (and whatever else the owner keeps in
v1.0). The owner tests on the Pi first, then Windows (#581); then annotated `v1.0.0` (#593).

---

## B. Candidates: owner decides v1.0 or later
Collected from PLAN "Parked", #594 (Keel's 1.1 draft) and our discussions. None is planned yet.

1. **Live editing for every module** beyond A2's list (e.g. module-state transfer when D5 is unavoidable).
2. **OSC Out to the LAN** (versioned config, `--confirm-lan`) and **OSC bundles with timestamps**
   (sample-accurate send times).
3. **MIDI Out** (notes/CC to devices; not discussed in detail yet).
4. **Groove start/stop feel** (#597 item 10), if not folded into A2.
5. **Pi 60-minute fake-device edit soak** #296 (needs a Claude session with `wirescape-pi`).
6. **R6: retire ABI1** #470 (engine cleanup after Qt).
7. **Web demo audio off the render thread** (AudioWorklet, #658 P1.2).
8. **Stereo Output module** #600.
9. **Oscillators: Macro gate input, pitch/Hz keyboard tracking, musical ranges** #337.
10. **sound.cross_mod** (delay phase modulation + AM/ring) #332.
11. **Benchmark regression gate** #91.
12. **Windows installer / packaged build** #436 (1.x stops being source-only).
13. **CHAOS roadmap** #491 and the **Quad VCA/LPG** follow-ups #490.
14. **SDK module-declared observation** (replace the host-internal pitch tap #580).
15. **Patch-library live refresh/publish** #428.
16. **CI speed:** parallel sanitizer lanes #588, the ARM64 ASan slowdown #585/#591.
17. **Static analysis:** pinned Plaits/stmlib diagnostics #361.
18. **Pi runner:** drop its Qt mode (private `wirescape-pi`, CLEANUP row 6).

---

## Done
(moved here when finished, with the PR numbers)

Signed: Keel, 2026-11-14
