<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC-2: Honest oscillator tuning display and musical dials (#342, from #337)

Tier T1. See #337 and the original Compass ruling in `docs/tasks/OSC_TUNING_GATE.md` (module-prep branch). No DSP edit, module/parameter ID or default change, or patch rewrite on load.

The VCO Frequency is a Hz value only without a pitch cable: show the selected value and nearest musical note/cents. With pitch connected, native DSP uses frequency/220 as a tuning ratio against C4, so show Tune in semitones/cents from 220 Hz, with the existing 220 Hz double-click reset. A shared equal-temperament helper uses A4=440 Hz/MIDI69, C4=261.6255653 Hz/MIDI60, G9/MIDI127. The VCO dial navigates C0–G9 with a logarithmic taper and labels both ends, while values outside remain available through typed input or existing saved patches, pinned visually at the dial edge without changing the numeric value. Macro coarse displays the selected note and cents relative to C4.

Tests cover note conversion, octave ratios, both connected/unconnected UI modes, saved/typed out-of-dial values, and native tracking of pitch 0=C4, 0.75=A4, 1.0=+1 octave, NOTE_ON 69=A4. Native tracking changes are **tests only**.

```acceptance
tier: T1
size: <= 600 changed lines, <= 6 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - cd apps/studio && npm ci && npm test
stops: [DSP edits, module/parameter ID or default change, patch rewrite on load]
```
