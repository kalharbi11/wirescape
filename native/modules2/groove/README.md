<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1(a) native core

`sequencing.groove`: two synchronized 32-step sixteenth-note tracks,
20–300 BPM, independent clocks per module instance. Editor: S1(b).
Persist version-1 JSON (old S0 patterns still load):
```json
{"version":1,"bpm":120,"tracks":[{"steps":[{"kind":"note","key":60,"velocity":0.8,"mod1":0.25,"mod2":0.75},{"kind":"tie"},{"kind":"rest"}]},{"armed":true,"mute":false,"steps":[{"kind":"note","key":67,"velocity":0.9}]}]}
```
Accept 1–2 tracks, 1–32 steps padded with rests; optional track
`armed` (true), `mute` (false); step `mod1/mod2` 0..1 (0).
Note key 0..127, velocity 0..1; zero velocity rests, tie sustains.
Unknown fields refuse; save JSON, not the 4×64 transient POD.
Slots 0–4 keep `t1_pitch/gate/mod1/mod2/notes`; 5–9 append `t2_*`.
Pitch 1V/oct C4=0, gate 0/1, mods 0..1 sample-held on admitted ON;
pitch/mods hold through rest/stop/reset. NOTE_OFF retry blocks new ON.
Edge params: `run`=0, `stop`=1, `reset`=2, `mute`=3,
`t1_run/stop/mute`=4/5/6, `t2_run/stop/mute`=7/8/9.
Initial high is baseline. Global/track STOP queues cycle boundary;
START arms/queues, MUTE releases while preserving phase, RESET releases
and clears queues. RESET > STOP > MUTE > START > tick; all same-frame
releases precede new notes.
Edit only after **native completed STOP**; off-render `loadGraph` ACK
precedes START, refusal retains draft. Apply may reset other modules.
S1(b) needs authoritative STOP/playhead observation; Pi/Windows and
owner audio acceptance remain release gates.
