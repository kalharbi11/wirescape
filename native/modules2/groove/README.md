<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1(a) native core

`sequencing.groove`: two 32-step sixteenth-note tracks, shared 20–300 BPM
audio-frame clock. UI editor is separate S1(b).

Version-1 persisted JSON (old S0 one-track patches remain valid):
```json
{"version":1,"bpm":120,"tracks":[{"steps":[{"kind":"note","key":60,"velocity":0.8,"mod1":0.25,"mod2":0.75},{"kind":"tie"},{"kind":"rest"}]},{"armed":true,"mute":false,"steps":[{"kind":"note","key":67,"velocity":0.9}]}]}
```
Accept 1–2 tracks, 1–32 steps padded to 32 with rests; optional
track `armed` (true), `mute` (false), step `mod1/mod2` 0..1 (0).
Note key 0..127 and velocity 0..1; zero velocity rests, tie sustains.
Reject unknown keys; save JSON, never the 4×64 transient POD.

Slots 0–4 keep `t1_pitch/gate/mod1/mod2/notes`; 5–9 append the
same `t2_*`. Pitch 1V/oct C4=0, gate 0/1, two mods 0..1 held on
admitted NOTE_ON; pitch/mods hold on rest/stop/reset. Events carry
balanced IDs; pending NOTE_OFF blocks new NOTE_ON.

Edge params: `run`=0, `stop`=1, `reset`=2, `mute`=3,
`t1_run/stop/mute`=4/5/6, `t2_run/stop/mute`=7/8/9.
Initial high is baseline. Global/track STOP queues 32-step boundary,
START arms/queues, MUTE immediately releases but retains phase,
RESET releases/clears queues and re-arms. Priority:
RESET > STOP > MUTE > START > tick. Same-frame releases precede ON.

Edit only after **native completed STOP**, apply candidate `loadGraph`
off render with ACK; refusal retains draft and prevents START. Apply
may reset unrelated modules. S1(b) must add authoritative STOP/playhead
observation and editor; hardware/Pi/Windows evidence remains gated.
