# CHORD / SEQUENCER — Wirescape API v1 independent module

Self-contained 16-step polyphonic chord sequencer with independent 16-step bipolar modulation lane. It outputs typed note-on/off events (not audio), trigger, gate, modulation and envelope CV. Runs with internal tempo or clock-edge advance; supports chord quality, inversion, voicing, strum, probability, play-every-N, sustain and reset.

## Install

Copy `modules/sequencer/` into the Wirescape 0.5+ `modules/` directory. In Wirescape, click **MODULES ↻ → SCAN MODULES**, review, then **ENABLE MODULE** while audio is stopped. New module activation loads `plugin.py` and automatically registers its ports/controls and custom editor assets. You need not modify host Python, HTML or global JS. Restart Wirescape after updating the code of a previously enabled module.

## Patch

- `NOTES` → CHAOS `NOTES IN` (polyphonic note on/off).
- `MOD` → CHAOS `CUTOFF IN`, `MORPH IN`, `PITCH IN` or `CHAOS IN`.
- `TRIG` → MODULATOR `CLOCK IN` or CHAOS `GATE IN` (the latter gates a fixed note rather than delivering polyphonic pitch).
- `CLOCK IN` advances the sequencer in EXTERNAL mode on a rising edge. `RESET IN` resets on a rising edge.
- CHAOS stereo audio → AUDIO I/O stereo input.

The sequencer's pattern is stored in the portable `module_state` field. Wirescape 0.5 can import earlier integrated `sequence` fields. The module uses the API v1 validated `configure()` state channel.

## Limitations

Events are dispatched at the start of audio blocks (~10.67ms at 48 kHz/512). Strum values smaller than a block are rounded to block boundaries. No note velocity, MIDI routing, MPE or sample-accurate scheduling. Python audio callback safety and Pi 5 polyphonic performance remain to be measured with real Surge hardware. The user-supplied previous integrated project had older Wirescape host code, so do not install its modified main files over Wirescape beta 0.4.
