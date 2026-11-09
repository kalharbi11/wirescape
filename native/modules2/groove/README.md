<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S0 module

`sequencing.groove` is a native **configuration proof**, not the S1 instrument.
Authored v2 patch `modules[].config` must be
`{"version":1,"bpm":120,"tracks":[{"steps":[{"kind":"note","key":60,"velocity":0.8},{"kind":"rest"}]}]}`.
One track with 1–8 sixteenth-note steps at 20–300 BPM is accepted.
Kinds are note, rest, tie; a note has integer MIDI key 0–127 and velocity
0–1. Velocity zero suppresses the note; ties sustain the previous note;
a tie without a sounding note is a rest. Steps loop. Unknown keys,
unsupported versions and invalid values are refused before activation.

S0 output slots are `t1_pitch` (V/oct, C4=0), `t1_gate` (0/1),
`t1_mod1` and `t1_mod2` (zero during S0), `t1_notes` (matched
NOTE_ON/OFF). The single scalar `run` parameter is a temporary
rising-edge proof control, not S1's queued transport. A fresh load is
always stopped, including a saved `run=1`; a new low→high edge starts
step 1. Falling edge releases notes. Reset releases via host choke.
Pitch holds the last note, gate drops on stop and mod outputs are zero.
If NOTE_OFF delivery fails, it is retried before another note can begin.

The patch stores structured JSON, never the internal C++ byte layout.
The runtime adapter parses off the render thread. Stopping this module
does **not** authorize mutating a graph whose render callback is active.
S1 will add 2×32 steps, two active mods, queued transport and a stopped
Studio editor/Apply acknowledgement. No live-edit, seamless Apply, or
finished instrument is claimed by this proof. Physical listening is
owner-only.
