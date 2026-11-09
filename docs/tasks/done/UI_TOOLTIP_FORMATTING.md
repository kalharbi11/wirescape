<!-- SPDX-License-Identifier: GPL-3.0-only -->
# UI tooltip range formatting (#185)

Presentation-only replacement for raw descriptor-bound float text in the Studio
knob and slider tooltips. Use three significant digits with Hz/kHz, percent,
dB and s/ms units. Do not change descriptor bounds, parameter IDs, accepted
values, rollback, Qt controls, DSP or saved patches.

The Windows Qt offscreen UI test instantiates a VCO knob and checks its
0.01 Hz–20 kHz tooltip, 440 Hz, percent, dB and millisecond/second ranges.
It also checks the slider tooltip source and that presentation does not
submit values to the controller.

```acceptance
tier: T1
size: <= 150 changed lines, <= 4 files
must_pass:
  - ci: windows-x64 studio_ui_tests includes humanReadableParameterTooltips (0.01 Hz, 20 kHz, 440 Hz, %, dB, ms) and VCO tooltip text
stops: [DSP or descriptor change, stable-ID change, rollback change, test removal, budget exceeded]
```
