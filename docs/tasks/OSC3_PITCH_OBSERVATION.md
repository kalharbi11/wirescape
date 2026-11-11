<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #337 OSC-3: nominal oscillator pitch observation (T2)

**Approved scope:** Keel's OSC-3 ruling on #337 (2026-11-10), after
OSC-1 #343 and OSC-2 #346 merged. One observation per module instance,
for the most recently triggered channel. This is **nominal, pre-FM**
pitch; no claim to measure an audible fundamental. An unpitched Plaits
model still has a nominal note, clearly labelled in Studio.

**Read first:** `AGENTS.md`, `apps/studio/AGENTS.md`,
`docs/tasks/OSC_TUNING_GATE.md`, issue #337, `native/modules2/vco/dsp.h`,
`native/modules2/macro_oscillator/dsp.h`, `native/engine2/engine2.cpp`,
`native/document/telemetry_taps.h`, `native/sidecar/protocol_tests.py`,
`native/web/web_engine_tests.cpp`, `apps/studio/src/panels/widgets/Knob.ts`,
`apps/studio/src/engine/FakeEngine.ts`, `docs/specs/ENGINE_PROTOCOL.md` §7.

**Implementation limits:**
- No new SDK/ABI, output port, module ID, parameter ID, saved patch field,
  DSP sound change, or runtime resource allocation on the audio thread.
- A bounded host-internal OscPitch tap stores one render-owned nominal base
  value, published with the existing lock-free block snapshot. Control-side
  sampler derives VCO MIDI pitch from Hertz; Macro publishes its nominal
  MIDI note. Both hosts publish one complete `effective_pitch` map at ≤30 Hz.
- VCO uses pitch CV + frequency/220 ratio, NOTE_ON/key selection or
  free-running frequency in the same precedence as DSP. FM is excluded.
  Macro uses 60 + coarse + clamped pitch CV, before its FM/model logic.
- Studio uses an additional, clearly qualified, read-only line under the
  relevant knob. Empty/missing/stale snapshots show an em dash, never a
  knob-derived substitute. FakeEngine publishes the same optional field.

**Acceptance evidence:** native engine2 tests enforce identical sample
bytes with/without the observation tap plus bounded/no-alloc reads, note
selection, note-off/choke and CV; native Web tests prove sample/clear/
replacement with VCO and Macro; sidecar protocol tests enforce the full
map, two module values and clear after graph swap; Playwright checks
formatting and empty-snapshot fallback. Follow the exact-head CI evidence
rather than inferring physical hardware acceptance.

```acceptance
tier: T2
size: <= 18 files, <= 450 changed lines
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
stops: [post-FM frequency claim, new module ABI/ports/IDs, audio-thread allocation or lock, stale UI pitch, DSP render mismatch]
```

Signed: ChatGPT (GPT-6 oscillator builder), 2026-11-10
