<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Phase 2: what's left (Compass, 2026-11-06 evening)

Phase 2 runs now, beside the owner's Test Session 1 (#271). Goal: **the Pi is an instrument** (Checkpoint B): a 30-minute jam with live patching, patch switching, no stuck notes, dropouts or crashes. Codex Lead cuts one issue per item below (in the style of #276: why, read first, files, budget, acceptance, tier) and keeps them parallel. Engine/realtime/protocol items are T2 and go to Keel for review. A Test Session 1 failure always jumps the queue.

**Status at writing:** 2A-1 #209 merged · 2A-2 #267/#273 in review · 2B-a on-screen keyboard #281/#283 · 2D-a patch folder #276/#279 · 2D-b autosave #277/#282 · 2E #264 merged · 2F-a soak harness #278/#284.

| Item | Scope | Tier | Read first | Depends on |
|---|---|---|---|---|
| **2B-b MIDI Learn** | Right-click a knob → "Learn", move a CC → the UI writes a `midi.cc_to_cv` row (D8; adds a module when 8 rows are full) and saves it in the patch; "Forget" removes it; `ui.ccMap` stays a display hint | T1 (UI) | `apps/studio/src/canvas/ModuleCard.ts`, `native/modules2/midi_cc_to_cv/README.md`, PLAN D8 | 2A-2 (topology edits apply live) |
| **2B-c MIDI hot-plug** | Unplug/replug the M8 while playing: the engine notices, releases held notes from that source, resubscribes when it returns, and pushes the new device list; the UI keeps the user's selection | T2 (engine) | `native/sidecar/DESIGN.md`, `docs/specs/ENGINE_PROTOCOL.md` (device lists, status), PLAN D7 | none |
| **2B-d keyboard default** | Global "computer keyboard plays" default with #184's semantics, through the engine | T1 | #184, `apps/studio` keyboard handling | 2B-a |
| **2B-e patchable MIDI controls** | Finish #210 (and #163 after it), additive IDs only | T1 | #210 handoff | none |
| **2C-a audio device loss** | Audio interface unplugged → status `no-device`, retry on a bounded backoff, reopen the remembered device when it returns; never fall back silently to another device. Fake-device test that removes and re-adds the device | T2 | `native/sidecar/DESIGN.md` (the "no-device state and retry" paragraph), `native/io/audio/` | none |
| **2C-b honest load and dropouts** | Status reports CPU as callback load (render time / period) and counts real dropouts; the UI shows both | T2 engine + T1 UI | ENGINE_PROTOCOL status, #274 histogram | #274 |
| **2C-c device details** | #200 semantics in the Qt-free service: mono devices, stale routes, the active route shown; optional sample-rate/buffer choice in Settings (48 k, 128–512 frames) that restarts only the stream | T2 | #200, `native/io/audio/DESIGN.md` | 2C-a |
| **2D-c patch browser** | A panel listing `patches/` plus bundled read-only starters (the 3 demo patches + the 6 starters); a click switches live (2A path); "Save as" from a starter | T1 | #276, #277, `apps/studio/src/patch/starters.ts` | 2D-a, 2D-b |
| **2D-d crash recovery** | After an app or engine crash the app offers to restore the last autosave; it never overwrites the user's file | T1 | #277 | 2D-b |
| **2F-b 60-min Pi soak** | Runner job in `kalharbi11/wirescape-pi`: engine with fake devices + #278's scripted edits for 60 min; zero crashes, faults and overruns; label `ci/pi-soak` | T1 | `docs/PI_RUNNER.md`, #164, #278 | 2F-a |

**Out of Phase 2 (don't start here):** OSC (3D, #82), telemetry v1 (3A), Windows (#266), Track S modules (their own lane, `docs/tasks/S1_CONJUNCTIONS_MODULES.md`).

**Phase 2 is done when** all rows are merged, the 2F-b soak is green on the Pi, and the owner's Checkpoint B jam passes (`docs/PLAN.md`).
