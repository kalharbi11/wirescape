<!-- SPDX-License-Identifier: GPL-3.0-only -->
# NOTES-TO-CV-421 — playable 3/4-voice integration contract and acceptance plan

Status: **authorized plan/test design only; production blocked on Keel** (2026-11-08).
Owner's first playable target is three and four simultaneous notes; a fifth is optional.
References: #421, existing Chord design #403 / draft PR #406, #401 and #405.
This task does **not** authorize a production module ID, an ABI/protocol change, or
Chord DSP. Work on #406 stays with its existing owner; keep #421 on its own branch.

## Read first (ground truth, no legacy or generated files)
- `AGENTS.md`, `docs/WORKFLOW.md` and #421's latest Codex Lead handoff.
- #403 / #406 latest contract/handoff; `docs/design/CHORD_GENERATOR_403.md`
  on `task/403-chord-contract` (proposal, not merged).
- `native/modules2/midi_to_cv/dsp.h`, `native/voice/note_to_cv.h`,
  `native/voice/allocator.h` and `native/voice/note_identity_tests.cpp`.
- `native/tests2/test_modules.h` (test-only `NotesToCv`),
  `native/control/module_catalog.{h,inc}`, `native/document/runtime_patch.cpp`,
  `native/web/web_engine.cpp`, Studio/FakeEngine catalogue and patch tests.

## Verified seam and prohibited shortcuts

- Shipped `midi.to_cv` accepts `WS2_MIDI` on `midi`, with 1–16 voices
  (default four); it does not receive `WS2_NOTE_ON/OFF` through a `notes`
  input. Do **not** wire `Chord.notes` into it or claim that succeeds.
- Test-only `ws2test::NotesToCv` in `native/tests2/test_modules.h` has a
  `WS2_EV_NOTES` input and poly `pitch/gate/velocity` outputs; it is not a
  production manifest/catalogue module and uses `voice::Allocator` directly.
- `native/voice/NoteToCv` already handles source/channel tokenization,
  anonymous IDs, steals, expressions and NOTE_ON/OFF ordering. Reuse its
  semantics after review rather than making a second unrelated allocator.
- The VCO event input uses assigned voice channels, not fresh chord voice
  allocation. A same-channel triad is **not** three sounding VCO voices.
- The sequencer's immutable `example.step_sequencer` ID, existing eight
  steps/slots 0–25, v1 state migration, and its #405/#410 ownership are out
  of scope. Quantizer CV is not a note-event allocator.

## Stop — Keel's written ruling must precede production edits

Keel: decide these questions for the #403/#406/#421 seam **before** adding
a public manifest, production DSP, routing or saved patch:

1. Are incoming `channel` fields assignment hints or source MIDI channel
   identities? Shall the adapter allocate fresh output voices regardless of
   incoming channel? Identify the output event channel and output CV lane
   contract, including repeated same-channel chord notes.
2. At a fan-in of two keyboards/chords, define how `source_id` and
   `note_id` pairs stay unique; whether source/channel/note identifies an
   activation; what a producer may do on ID reuse; and whether remapping or
   a source-namespace change is required. `note_id=0` is valid.
3. Define negative/anonymous IDs, repeated same-key NOTE_ON, ambiguous
   delayed NOTE_OFF after steals or ID reuse, retirement/tombstones,
   source removal, and wildcard/targeted `WS2_NOTE_CHOKE` behavior.
4. Specify pressure/timbre/`WS2_NOTE_EXPRESSION` mapping through Chord
   output IDs and allocator voices; do not forward an unmatched input ID.
5. Set fixed bounds for activations, output voices, pending releases and
   rejected emits; define priority, partial attack rollback, steal ordering,
   retry/panic and saturation behavior without losing NOTE_OFFs.
6. Authorize a separately registered Notes→CV production module (preferred
   bounded `voice::NoteToCv` reuse) **or** a reviewed alternative, including
   stable public ID/port/parameter/version proposal. No speculative IDs here.
   Any engine/ABI/protocol semantics change is a **separate T2** task and
   cross-family Claude review, not an implicit addition to this T1 slice.

Codex Lead coordinates with the existing #406 design owner. Compass retains
the already confirmed three/four-note product priority; no strum or fifth
voice prerequisite. Keep draft until a signed Keel ruling is linked.

## Post-ruling implementation sequence (NOT authorized in this slice)

1. **Native allocator:** add the reviewed module/manifest/registration using
   the bounded voice service. Its `notes` input must accept
   `WS2_NOTE_ON/OFF/CHOKE/EXPRESSION`, and its real poly
   `pitch/gate/velocity` outputs must have four distinct lanes and
   sample-accurate transitions. No allocation, locks, logs, exceptions, JSON
   or devices in process/reset. Never alter `midi.to_cv` behavior to pretend
   it accepts `WS2_EV_NOTES`.
2. **Native graph:** null-sink/fake-MIDI `AppEngine` tests verify direct
   four-note MIDI through the existing MIDI→CV baseline separately from
   `Keyboard/Sequencer.notes → Chord.notes → Notes→CV → poly VCO/ADSR/VCA →
   Output`. Inspect both event traces **and** actual per-voice CV/audio.
3. **Registration/persistence:** register under the approved public identity,
   synchronize native catalogue, Studio presentation/generated catalogue,
   FakeEngine and versioned patch save/reload/undo. No undocumented implicit
   migration or discarded unknown fields. Keep #406 and #405 independent.
4. **WASM/Studio:** load the same real engine through
   `native/web/web_engine.cpp` and Web AudioWorklet; verify actual
   render-backed pitch/gate/velocity channels and nonzero finite stereo
   audio for triad and seventh, rather than a UI mock or a test-only adapter.
   Check browser catalogue, load/reload and no duplicate unquantized pitch
   path. Exercise Electron sidecar with null sink separately.
5. **Evidence/review:** exact-head guard, linux-arm64, windows-x64,
   sanitizers, Web/Playwright, native CTests, and device-free fake-device Pi
   build when the implementation PR requests it. Independent T1 review;
   route any engine/ABI/protocol change as T2 with Keel's ruling. Physical
   audio/MIDI/display is owner-only and never claimed from CI.

## Executable test matrix (fixtures/specifications; not yet passing tests)

| Input/edge | Real native observation | Real WASM/Studio observation |
|---|---|---|
| Root 60, triad 0/+4/+7 | simultaneous keys 60/64/67, unique output IDs; lanes 0/4/7 semitones relative to MIDI 60, gate=1, copied velocity | three concurrently active pitch/gate lanes; actual finite nonzero audio |
| Root 60, seventh 0/+4/+7/+11 | keys 60/64/67/71 at same sample offset, four separately allocated lanes, 4 balanced offs | four concurrent voices and independently falling gates; nonzero rendered stereo |
| Direct MIDI baseline | four held MIDI notes through shipped `midi.to_cv`, no Chord; all four pitch/gate lanes | real WASM baseline separate from generated chords |
| Overlapping/repeated roots | two sources, channel 0/15, note_id 0 and positive/anonymous IDs, repeated keys; each off releases only its activation under Keel rule | no replacement note killed by stale offs |
| Edits/sustain/steal | held interval edits affect next activation only; sustain and four-voice steal/reuse, per-note expressions and order checked | live edit, save/reload, panic/replan/source removal clear all gates |
| Queue/refusal | reject 1st/middle/last attack and one/many offs, retry across blocks, overflow and choke/panic; no leaked gates | graph reload cannot leave a stuck output note |
| Determinism | 44.1/48/96 kHz, blocks 1/17/128/max; same absolute event offsets, finite output, no process allocations/faults | real engine output channels and finite nonzero sound after reload |

Golden pitch units are **1.0/octave and C4 (MIDI 60) = 0**:
triad pitches `[0,4/12,7/12]`; seventh adds `11/12`. Check emitted
velocity and channel/gate transitions per voice, not just module existence.
Confirm distinct nonzero stereo contribution or isolated per-voice renders,
so an apparent four-note UI cannot pass with one audible oscillator.
Tie/wrap/steps 9–16 remain #405/#410 acceptance, not new #421 DSP.

## Plan-only acceptance and handoff gate

This first PR adds *only this authorized task plan*. The executable integration
CTest/Playwright targets above are **future deliverables**, and must not be
reported as passing before implementation and Keel's decision. A `Refs #421`
draft intentionally does not close the production issue; verify the
acceptance commands explicitly if hosted CI does not select tasks without
`Closes #N`. Do not misreport a green, skipped task job as acceptance.

Signed: ChatGPT (GPT-6, Notes→CV task/test-plan builder), 2026-11-08

```acceptance
tier: T1
size: <= 250 changed lines, <= 3 files
must_pass:
  - python -c 'from pathlib import Path; t=Path("docs/tasks/NOTES_TO_CV_421.md").read_text(); assert all(x in t for x in ("Keel", "note_id=0", "WS2_NOTE_OFF", "WS2_NOTE_CHOKE", "WS2_NOTE_EXPRESSION", "WASM", "44.1/48/96", "60/64/67/71"))'
  - bash -lc 'git diff --check origin/main...HEAD'
  - ci: guard, linux-arm64, windows-x64 and sanitizers on exact plan head
stops: [public ID or ABI/protocol edit, engine/Chord production edit before Keel ruling, conflicting #406 ownership, test weakening, new dependency]
```
