<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHORD-403: incoming-note chord generator — proposed contract (design only)

Status: **proposal for Keel/Codex Lead review**, 2026-11-08. No DSP,
catalogue, panel or ABI implementation is authorized by this note. This is
an event transformer, **not** a chord-pattern sequencer or an Ableton clone.
It is independent of #401's 16-step sequencer and #404's parameter cap.

## Public surface to confirm

- Proposed stable module ID: `essentials.chord`; display name: **Chord**.
  Confirm this exact ID before publishing a manifest or saved patch.
- One `notes` input and one `notes` output, both `WS2_EVENT` with
  `WS2_EV_NOTES` hint and `WS2_POLY` semantics; no signal/CV ports in v1.
- Eight stable stepped, automatable parameters in this proposed order:
  `enable1`, `shift1`, ..., `enable4`, `shift4`. Each enable is
  0/1; each shift is an integer semitone offset in [-24,+24].
  Defaults: (on,+4), (on,+7), (off,+12), (off,-12).
  The untransposed root is **always** present, so defaults sound three
  notes (0,+4,+7), and enabling the other slots yields four or five.
- Parameter changes apply only to **subsequent NOTE_ONs**. Each activation
  snapshots its actual emitted pitches; a held chord is not retuned.
  Duplicate offsets, including zero, deliberately create separate voices
  with distinct output note IDs; do not deduplicate by pitch.
- Keys are MIDI integers 0..127. A shifted key outside that range is
  **omitted**, not clamped into an accidental duplicate; the valid root
  remains. Velocity is copied exactly (subject to ABI validity).
- Version 1 runtime state is transient; host patch parameters persist
  separately. Save/load must never resurrect held notes or bypass host
  reset/choke behavior. No internal clock, strum, arpeggiator, scale or
  transport dependence. No automatic Quantizer insertion.

## Event ownership and timing

- Input `NOTE_ON` key K at sample offset t emits root K and each enabled
  in-range K+shift at **the same offset t**, preserving input velocity and
  channel. Each output note receives a unique nonnegative `note_id` from
  a bounded module-local sequence, not the input ID. Store the actual
  emitted (key, output note_id, source_id, channel) per input activation.
- Match a positive input `note_id` by (source_id, channel, note_id).
  For anonymous negative IDs, track activations by (source_id, channel,
  key) and consume matching repeated NOTE_OFFs in a deterministic LIFO
  order. Repeated NOTE_ONs are distinct activations, not a pitch toggle.
  A late NOTE_OFF for a retired/stolen activation must not release a
  newer activation with the same pitch.
- On `WS2_NOTE_OFF`, emit one off per **actually emitted** output voice,
  at the input offset, with its stored ID/key/channel/source_id. Releases
  precede any replacement attacks at equal sample offsets. Ignore
  unmatched offs without sending a broad wildcard release.
- On `WS2_NOTE_CHOKE`, release/choke only matching source activations;
  `source_id=UINT32_MAX` chokes all. On reset, stop, disconnect, graph
  replacement and patch reload, follow the existing engine's quiescent
  choke contract; never assume `reset()` can emit an event.
- `NOTE_EXPRESSION` must **not** be forwarded with the original input
  note_id (which downstream cannot match). Proposed v1 behavior: map
  supported per-note expressions to all active emitted IDs for that
  activation, preserving offset; unsupported expressions are an explicit
  Keel contract question, not silently reinterpreted.
- Preserve input `source_id` on output as existing note processors do.
  **Keel stop:** ABI2 D5 permits multiple event sources into one input;
  `engine2::route` forwards source IDs unchanged. Module-local output
  note IDs may collide with another transformer sharing that source ID
  at downstream fan-in. Confirm a safe identity namespace/routing rule
  before implementing. Do not invent an ABI field or synthesize arbitrary
  source IDs in a public contract.

## Bounded failure policy to validate

- Candidate capacity: at most **16 active input activations** and five
  emitted notes per activation (80 voices maximum); fixed arrays and
  bounded scans, no process allocation, locks, I/O or unbounded retries.
  This capacity is proposed, not a guaranteed host voice limit.
- Reserve enough fixed pending-release records for every successfully
  emitted note (80). Flush pending NOTE_OFF/CHOKE before new attacks and
  at the start of subsequent blocks, retaining each failed release when
  `b.emit` reports `WS2_CAPACITY`. Never overwrite a pending off.
- On full activation/pending capacity, decline a new NOTE_ON; existing
  releases have priority. A partially emitted chord under event-queue
  exhaustion retains only the successful voices, then releases exactly
  those; never record a note-on that was rejected. A refused off remains
  pending, not forgotten. Stop admitting new notes if safe release
  bookkeeping cannot be reserved. Check worst-case bounded work at
  1/17/128/max frames, and across repeated capacity refusals.
- Sequence wrap must avoid reusing any output ID still active or pending.
  Test after wrap and source/channel churn. `WS2_NOTE_CHOKE` wildcard
  must not erase unrelated upstream notes at downstream fan-in.

## Tests (planned, not executed)

| Test | Required observation |
|---|---|
| Major root 60 | `NOTE_ON` keys 60/64/67 at identical sample offset and velocity; three distinct output IDs; matching three offs |
| Minor/custom | shifts -3/+7, enabled slots 3–4, negative and high-key out-of-range omissions; exactly 3–5 in-range voices including root |
| Identity | two sources, channels 0/15, repeated root and anonymous IDs, shared output pitches, LIFO offs; no cross-release |
| Held edits | change enable/shift while held; old pitches release exactly, next activation uses new settings |
| Choke/reset | targeted and wildcard choke, reset, stop, graph removal, patch reload; no stranded voices or false resurrection |
| Capacity | refuse first/middle/later note-on, refuse releases over multiple blocks, exhaust activation slots, retry with off priority |
| Realtime | 1/17/128/max frames, 44.1/48/96 kHz, no allocations in `process`, bounded work and deterministic output |
| Integration | keyboard -> Chord -> MIDI-to-CV/allocator -> instrument; Step Sequencer `notes` -> Chord; FakeEngine catalogue/panel and save/reload |

Native proposed targets: `abi2_chord_tests` (callback sink, deterministic
event sequences) and `abi2_chord_engine_tests` (real engine routing,
note identity, graph churn). Both are **future** targets, not current CTest
evidence. Later implementation must pass headless/Windows/ARM64/sanitizers
and fake-device Pi runner where required; no physical device access.

Quantizer is optional: event-note quantization **before** Chord changes
the root first, whereas pitch-CV quantization **after** MIDI-to-CV can
change individual sounding pitches without rewriting note-event keys.
Document patch wiring explicitly; do not claim these paths are equivalent.

## Lead decisions before implementation

1. **Keel:** approve or revise the source_id/note_id fan-in namespace,
   anonymous matching, expression forwarding, release queue and 16/80
   capacity. If this needs engine/ABI/protocol work, stop and split T2.
2. **Codex Lead/Compass:** confirm `essentials.chord`, the two `notes`
   ports, eight IDs/ranges/defaults, duplicate and out-of-range policy.
3. After these rulings, open a separate bounded implementation task/PR
   with native deterministic tests first, then manifest/UI/FakeEngine
   coverage and the normal T1 different-session review.

Source seams inspected: `native/include/wirescape/module2.h` event
fields; `docs/specs/ABI2_PLAN.md` D5/D8 fan-in; `native/engine2/engine2.cpp`
`Impl::route`; `native/sdk/examples/step_sequencer/dsp.h` pending-off
handling; `native/voice/note_to_cv.h` anonymous IDs and release; #403.
This document is a proposal, not a claim of tests or runtime support.
