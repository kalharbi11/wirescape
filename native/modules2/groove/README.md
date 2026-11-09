<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1(a) native core

`sequencing.groove` provides two synchronized 32-step sixteenth-note tracks
(20–300 BPM) with independent per-module clocks. S1(b) supplies the Studio editor.

Persist JSON `module.config.version=1`, **not** the 4×64 transient POD:
```json
{"version":1,"bpm":120,"tracks":[
 {"steps":[{"kind":"note","key":60,"velocity":0.8,"mod1":0.25,"mod2":0.75},{"kind":"tie"},{"kind":"rest"}]},
 {"armed":true,"mute":false,"steps":[{"kind":"note","key":67,"velocity":0.9}]}
]}
```
Old S0 one-track patches remain valid. Accept 1–2 tracks, 1–32 steps
(pad shorter tracks with rests), optional track `armed` true and `mute` false,
optional step `mod1`/`mod2` 0..1 default zero. Notes need MIDI `key` 0..127
and `velocity` 0..1; zero velocity rests, tie sustains. Unknown fields refuse.

Permanent slots: `t1_pitch/gate/mod1/mod2/notes` (0–4), then
`t2_pitch/gate/mod1/mod2/notes` (5–9); pitch 1V/oct C4=0, gate 0/1,
mods 0..1 sample-held on admitted NOTE_ON. Pitch/mods hold on rest,
tie, stop, reset; disabled track is zero. NOTE_OFF retry blocks later ON.

Edge parameters by slot: `run` 0, `stop` 1, `reset` 2, `mute` 3,
`t1_run/stop/mute` 4/5/6, `t2_run/stop/mute` 7/8/9.
First observed high is baseline, not START. Global STOP toggles a
shared-step-32 boundary stop; track STOP queues its own exit, track START
arms/queues entry; repeated actions toggle. Mute immediately releases
but preserves phase. Reset releases, clears queues and waits for the
next step-1 boundary. Priority: RESET > STOP > MUTE > START > tick.

Draft edits require **native confirmed STOP**. Submit a validated candidate
via off-render `loadGraph`, await load ACK, issue fresh START on success,
and retain draft/do not start on refusal. Reload may interrupt other modules.
S1(b) owns UI and truthful native STOP/playhead observation; no new protocol
or live-edit seam is implemented here. Platform/Pi and owner audio remain
release gates.
