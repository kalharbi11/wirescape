<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHORD-403: incoming-note chord generator contract and test plan

Independent #403 design-only slice. No runtime implementation or catalogue
registration until the lead confirms the proposed IDs and event-ownership
contract. Do not read/copy the legacy chord sequencer.

## Read first
- AGENTS.md, #403 and its current handoff, docs/PLAN.md "Now".
- docs/SDK_AUTHOR_GUIDE.md (note-event and SDK contracts).
- native/include/wirescape/module2.h, docs/specs/ABI2_PLAN.md note events.
- native/sdk/examples/step_sequencer/dsp.h and tests.cpp (event-off retry).
- native/voice/note_to_cv.h, native/voice/note_identity_tests.cpp.
- native/engine2/engine2.cpp event routing, native/modules2/midi_to_cv/dsp.h.

## Scope
- Propose a stable module ID, ports, eight simple controls and defaults.
- Describe root passthrough, up to four enabled semitone offsets, MIDI note
  identity, repeated/overlapping notes, note-off/choke and interval edits.
- Specify deterministic bounded active/pending capacity and event failure
  handling without allocating in process or changing ABI/protocol.
- Record exact input/output event fields, offset/velocity/voice policy,
  duplicate intervals, reset/reload and note-expression caveats.
- Provide a focused native/FakeEngine/hosted/Pi test plan and Keel stops.
- Add only this authorized task and docs/design/CHORD_GENERATOR_403.md.
- No module DSP, manifests, UI, #401 files, new dependencies or devices.

```acceptance
tier: T1
budget: one independent design session
size: <= 250 changed lines, <= 3 files
must_pass:
  - python -c 'from pathlib import Path; p=Path("docs/design/CHORD_GENERATOR_403.md"); t=p.read_text(); assert all(x in t for x in ("source_id", "note_id", "WS2_NOTE_OFF", "WS2_NOTE_CHOKE", "WS2_CAPACITY", "Keel", "Tests"))'
  - bash -lc 'git diff --check origin/main...HEAD'
  - ci: guard, linux-arm64, windows-x64, sanitizers green on exact PR head
stops: [ABI or protocol change, ambiguous event ownership, realtime unbounded work, dependency, test removal, catalogue or module implementation]
```

## Handoff gate
A different-session review is required for this T1 design. Lead confirmation
of the ID and event identity semantics is required before an implementation
task/PR. No hardware or native execution is claimed by this documentation.
