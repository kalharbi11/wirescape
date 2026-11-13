<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #724 MIDI to CV: pitch and gate reach VCO/ADSR for every note (priority 1)

Read first: #724 (Keel's repro notes), `native/modules2/midi_to_cv/dsp.h`, `native/modules2/notes_to_cv/dsp.h`, `native/voice/allocator.*`, `native/io/midi/midi_service.cpp`, `native/sidecar/protocol_unit_tests.cpp` (fake MIDI `inject`), the Studio connect path for signal cables, `docs/PATCH_DESIGNER_GUIDE.md` (MIDI section).

1. Reproduce device-off: a native test drives the fake MIDI backend → `midi.in` → `midi.to_cv` → `essentials.vco` pitch + `essentials.adsr` gate, five different notes with note-offs, default parameters. Assert the VCO's rendered frequency follows each note and the ADSR's gate opens and closes for each one. Repeat via `notes` → `essentials.notes_to_cv`.
2. Check the Studio side: dragging `pitch` → VCO `pitch` and `gate` → ADSR `gate` creates `signal` edges to those ports (not a modulation of a parameter), and the saved patch matches.
3. Fix the actual cause found. If it's poly channels read as channel 0 by a mono path, the fix goes where the spec says (ABI2_PLAN "Poly into mono": SUM/FIRST/POLY), not by changing module IDs or defaults silently. A default change needs Keel.
4. Use the owner's patch file on #724 when it arrives, and add it as a fixture.

```acceptance
tier: T1
size: <= 350 changed lines, <= 7 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - bash -c 'cd apps/studio && npm ci && npm test'
  - bash -c 'git diff --check origin/main...HEAD'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [module/port/parameter ID change, default-value change without Keel, ABI header edit]
```

Patch guide impact: update the MIDI to CV wiring note if the fix changes how poly pitch/gate feed mono modules.
Signed: Keel, 2026-11-13
