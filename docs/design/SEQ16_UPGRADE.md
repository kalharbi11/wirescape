<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Step Sequencer 16 — compatibility-first upgrade proposal (#401)

Owner request: upgrade **`example.step_sequencer`** from eight to sixteen steps, not a parallel
"Basic Sequencer" identity. Work in milestones: (A) 16-step notes/gates/pitch/velocity,
(B) optional richer timing, (C) chords in a separate researched issue. Strum is not a
condition of (A) or (B). This document is a reviewable implementation brief, **not an
ABI authorization or an assertion that 16-step DSP already works**.

## Evidence and existing contract

- `native/sdk/examples/step_sequencer/dsp.h` is running native DSP: 8 steps, external
  Schmitt clock/reset, note/rest/tie, per-step pitch + velocity, note-on/off events,
  pitch/gate/velocity CV, a pending-note-off retry, and version-1 playback state.
- `native/document/builtin_source_tests.cpp` verifies real Clock → Sequencer notes →
  VCO render without allocations, and `native/sdk/examples/step_sequencer/tests.cpp`
  covers block-size matrix, tie, reset, note-off overflow, state and finite values.
- `apps/studio/src/panels/widgets/StepsPane.ts` edits params via the ordinary
  host/engine path; `sequence-panel.spec.ts` covers undo, keyboard input, save/reload.
- The public module ID is `example.step_sequencer`, version 1; published ports are
  `clock`, `reset`; `pitch`, `gate`, `velocity`, `notes`. Existing parameter slot
  identities are exactly `run:0`, `length:1`,
  `pitch1:2, gate1:3, vel1:4` through `pitch8:23, gate8:24, vel8:25`.
  **Do not shift or reinterpret these.**

## Technical decision needed from Keel

`WS2_MAX_PARAMS` is **32** in the ABI 2 public header and the SDK's `prepare`
rejects a module whose parameter count exceeds it. The correct straightforward
extension requires **26 + (8 × 3) = 50 parameters**, *before* a tempo, swing,
division or mode control. A two-row UI does not remove this engine limit.

**Keel's ruling (#401, 2026-11-08):** implement a separate, additive host
parameter-cap change in #404 (32 -> 64; authored by Keel, T2) after its
cross-family review and CI. Parameter storage is dynamic; all fifty step
parameters use ordinary host parameter automation and patch persistence.
The sequencer module change is T1, stacked on the #404 branch pending merge.
Do not implement opaque float packing, bank aliases, a new patch-state
contract or any ABI edits in the module PR.

### Planned additive parameter slots (approved; #404 required)

| Slots | Exact IDs | Behavior |
|---|---|---|
| 0–25 | `run`, `length`, and existing `pitch1/gate1/vel1` through `pitch8/gate8/vel8` | **unchanged**, including defaults and 1/8 compatibility |
| 26–49 | `pitch9/gate9/vel9` through `pitch16/gate16/vel16` | same ranges, units and per-step semantics as 1–8 |
| 50+ (separate timing slice) | proposed `clock_mode`, `tempo_bpm`, `division`, `swing_pct`, `gate_length_pct` | freeze exact IDs/order only with Keel/Compass |

**Keep `length` default 8**, even for a new version of the same module.
Old patches may omit an explicit length and must still sound identical.
The updated editor can render 16 available steps while dimming 9–16 when length=8.
New factory starters can explicitly set length 16. The existing parameter
index formula `2 + 3 * step` remains valid for step indexes 0–15 because all
new triples are appended contiguously after slot 25.

## Playback and compatibility

1. The existing audio-thread process path remains sample-accurate, bounded and
   allocation-free, with Schmitt clock/reset and channel-zero note event ownership.
   Each rising clock plays exactly one current step; index wraps at `length`.
2. Rest emits no NOTE_ON; tie with sounding note holds pitch/gate without
   retrigger and a tie with no sounding note stays silent. Gate-off on clock fall
   must look ahead at the next step, including 8→9, 16→1 and shortened lengths.
3. On an ON step, release the previous note (NOTE_OFF first) before NOTE_ON.
   If the event output is full, postpone NOTE_OFF and **do not send a new NOTE_ON**
   until that OFF is accepted. Reset, stop and teardown must never strand notes.
4. Preserve the existing version-1 reset edge behavior for legacy patches until
   Keel resolves a mismatch: the current eight-step module processes a same-frame
   reset then clock and plays step 1, whereas the 2026-10-27 Beta brief says
   reset should **ignore** that clock. Do not silently change this in a 16-step
   patch. If changed, that is a separately reviewed behavior migration with
   legacy golden fixtures.
5. State v1 must load into v2 (with valid step 0–7); v2 state permits step 0–15.
   Patch params own pattern values; playback position is runtime state. Restored
   active notes are released before further notes begin. Reject corrupt state
   without partially committing it.
6. Preserve the sample-level emitted-event trace for all current eight-step
   test fixtures. Add 9→10, 15→16→1, length 8→16→4 while running, rests/ties
   at the boundary, reset/stop, event-capacity refusal, hot edits and save/load.
   Repeat at 44.1/48/96 kHz and blocks of 1,17,128,max frames. Monitor output
   bounds and allocation count.

## Editor requirements

- Use the same **StepsPane** and public module ID. Derive the available count
  from the engine's actual descriptor; never fabricate unregistered controls.
- Eight columns × two rows at 16 steps. Existing eight-step appearance, test IDs
  and editing behavior must not regress. Numeric pitch, gate-mode cycle and
  velocity remain directly editable for **every** step.
- A step beyond the current `length` is dimmed, not deleted; shortening length
  cannot destroy the saved pattern. Undo/redo, keyboard entry, save/load and
  engine refusal rollbacks work for steps 9–16 too.
- No animated playhead prediction from wall-clock time. Read engine telemetry;
  a stopped transport clears the playing indicator. A missing real-telemetry
  field is **unverified**, not permission to invent playback state.
- UI prep on `task/401-step-sequencer-16` is descriptor-driven and exercises an
  extended catalogue in Playwright only. It does not ship a fake 16-step engine.

## Quantizer interoperability (#315 / #323)

The Quantizer is `essentials.quantizer` (its own module lane, still in review
at the start of #401), with `pitch` signal input and `pitch` signal output,
scale/root/shift/octave, polyphony 1–16, sampled or continuous mode, and
`changed` trigger on pitch **change**. Wirescape pitch is **1/oct, 0=C4**.

**Correct musical wiring for the live signal path** (without a duplicate
unquantized event pitch driving the synth):

```text
Clock.clock → StepSequencer.clock
StepSequencer.pitch → Quantizer.pitch → VCO.pitch
StepSequencer.gate → ADSR.gate; ADSR.envelope → VCA.gain_cv
VCO.audio → VCA.audio → Output.audio
```

(Use the actual VCA and Output port IDs from the live module descriptors in
the golden test.) VCO prioritizes a connected pitch CV for frequency over a
`notes` event: do not rely on both to imply "quantized note events."
The Quantizer's `changed` is **not** equivalent to a sequencer gate: two
consecutive steps producing the same quantized pitch may not emit a changed
trigger, and a tie must not retrigger the envelope. Prefer sequencer gate.

Add a device-free graph test after #323 is merged (or on a separate exact-head
integration branch without touching its changes):

- C4 `pitch=0` → Quantizer C-major → C4; C#4 `pitch=1/12` →
  C4 (ties down); D4 `pitch=2/12` → D4; all pitch in octaves.
- 16-step sequence with notes outside C-major and repeated quantized pitches:
  check exact VCO pitch/frequency, **gate timing** and 16-step wrapping.
- Separate raw event-path test: Sequencer.notes → VCO.notes, with expected
  unquantized MIDI keys and balanced NOTE_ON/OFF. If the user wants *quantized
  note events*, build an explicit note-event quantizer/bridge; never claim
  Quantizer CV has rewritten Sequencer.notes.

## Timing slice after 16-step parity

Native external Clock already supports BPM and ticks-per-beat and should stay
the simplest mode. Internal BPM, swing, gate-length and division require explicit
clock-source selection, stable sample-domain scheduling across irregular blocks
and deterministic phase on edits/reset; do not bolt them into the current
clock-edge code without golden timing tests. Swing at 60% should produce
7200/4800-frame pairs for 120 BPM, 1/16 division at 48 kHz.
At a patched external clock, external rising edges have precedence. Do not
secretly apply internal swing to external edges. This may be a follow-up PR
after core parity if the size/review budget requires splitting.

## Review and delivery

1. Keel approved #404 (64 parameters) and Compass approved the compatibility scope; preserve existing reset behavior for the 16-step upgrade.
2. Codex Lead coordinates an independent review and acceptance, Compass checks
   the 16-step interaction and timing priorities.
3. A T2 implementation PR must pass device-free headless CTests, studio
   Playwright, catalogue sync, Pi runner as applicable, and an owner-operated Pi
   session for actual sound. An unrun check is not a pass.
4. Only once this sequencer branch is handed off, open a **separate** chord
   research/design issue. The chord module should process 3–5 voices from a
   root note and a selectable quality, independently of strumming.

Signed: ChatGPT (GPT-6, sequencer builder), 2026-11-08
