<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1(a) native module

 `sequencing.groove` is a two-track 32-sixteenth-step native instrument.
Authored v2 patch `modules[].config` remains version 1:
`{"version":1,"bpm":120,"tracks":[{"steps":[{"kind":"note","key":60,"velocity":0.8,"mod1":0.25,"mod2":0.75},{"kind":"rest"}]}]}`.
One or two tracks of 1–32 authored steps are accepted; missing steps pad
with rests, both tracks share the same 32-step boundary and 20–300 BPM.
Optional per-track `armed` (true), `mute` (false), step `mod1`/`mod2`
(0..1, default 0). Saved S0 patterns remain valid.
Kinds are note, rest, tie; a note has integer MIDI key 0–127 and velocity
0–1. Velocity zero suppresses the note; ties sustain the previous note;
a tie without a sounding note is a rest. Steps loop. Unknown keys,
unsupported versions and invalid values are refused before activation.

S0 output slots remain `t1_pitch` (V/oct, C4=0), `t1_gate` (0/1),
`t1_mod1`, `t1_mod2` (sample-and-hold 0..1), `t1_notes` (matched
NOTE_ON/OFF). Five new slots append `t2_pitch/gate/mod1/mod2/notes`.
Both mods latch on admitted NOTE_ON; rest/tie/stop/reset hold pitch/mods.
A fresh load is always stopped, including saved `run=1`; only a fresh
low→high edge starts step 1. Pending NOTE_OFF blocks new NOTE_ON.

Scalar IDs: `run` 0, `stop` 1, `reset` 2, `mute` 3,
`t1_run/stop/mute` 4/5/6, `t2_run/stop/mute` 7/8/9.
All are rising-edge commands after first-sample baseline. Global/track
STOP queues until the shared step-32 boundary; START arms/queues;
MUTE releases immediately without losing phase; RESET releases and
clears queues. Priority RESET > STOP > MUTE > START > step advance.
All same-frame releases occur before any new NOTE_ON.

The patch stores structured JSON, never the internal C++ byte layout.
The runtime adapter parses off the render thread. Stopping this module
does **not** authorize mutating a graph whose render callback is active.
The Studio editor and native authoritative STOP/playhead observation are
S1(b) work. No live-edit, seamless Apply or physical listening is claimed.

## Stopped configuration workflow (manual S0 / S1 UI contract)

**Confirm global sequencer STOP has completed**, not merely been requested
or queued. Edit an **authored draft** `module.config` while stopped; never
change the render-owned module state directly. Apply by submitting a
candidate through the existing off-render `loadGraph`/config reload
transaction, then await the **native load ACK**. On success the new graph
starts stopped; issue a fresh Start to play from step 1. On a native
refusal retain the draft and report the refusal; **never start a rejected
candidate**. Existing graph reload may interrupt/reset other modules,
so seamless Apply and phase continuity are later work. S1(a) still exposes only this manual contract: the graphical editor,
global STOP acknowledgement and truthful native playhead are S1(b).

Version 1 config is extended additively without changing old field meaning.
The validated 4×64 internal POD is transient and never serialized.
Live per-step editing is deferred to S2 or explicitly S3.
