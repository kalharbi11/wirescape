<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1(a) native core

`sequencing.groove`: two synchronized 32-step/sixteenth-note tracks at
20–300 BPM; one two-bar master cycle, independent clocks per module instance.
This is the native core; the stopped Studio editor is separate S1(b).

## Authored `module.config` (persisted JSON, never POD)

```json
{"version":1,"bpm":120,"tracks":[
 {"steps":[{"kind":"note","key":60,"velocity":0.8,"mod1":0.25,"mod2":0.75},{"kind":"tie"},{"kind":"rest"}]},
 {"armed":true,"mute":false,"steps":[{"kind":"note","key":67,"velocity":0.9}]}
]}
```

S0's one-track version 1 JSON is still valid. Accept 1–2 tracks and 1–32
steps/track; pad shorter tracks with rests up to 32, without rewriting the
authored patch. Optional track `armed`=true and `mute`=false; optional
step `mod1`/`mod2` in 0..1 (defaults 0). Note requires MIDI
`key` 0..127 and `velocity` 0..1; zero velocity is a rest. Tie sustains
the previous note; rest releases. Unknown fields/types/ranges are refused.

Output slots 0–4 retain `t1_pitch/gate/mod1/mod2/notes`; slots 5–9
append `t2_pitch/gate/mod1/mod2/notes`. Pitch is 1 V/oct (C4=0);
gate is 0/1; both mods sample-hold 0..1 only on successfully emitted
NOTE_ON. Pitch and mods hold through rest/tie/stop/reset. Disabled
track 2 outputs zero. Notes use matched IDs and deferred NOTE_OFF retry.

Parameter slots are `run`=0 (published S0 ID), `stop`=1,
`reset`=2, `mute`=3, `t1_run`=4, `t1_stop`=5,
`t1_mute`=6, `t2_run`=7, `t2_stop`=8, `t2_mute`=9.
All commands need a fresh low→high edge after an initial saved-value
baseline; high on load is never START. Global START plays step 1;
global STOP toggles queue for the shared cycle end. Track START arms
while globally stopped or queues entry at the next cycle boundary;
track STOP queues exit (or cancels queued entry), and repeated
requests toggle pending actions. MUTE toggles immediately, releases
notes/gates without losing phase, and unmute awaits the next eligible
note. RESET releases/clears queues, preserves running/armed/mute intent,
then waits one sixteenth for step 1 if running. Same-frame precedence:
RESET > STOP > MUTE > START > step tick; releases before note-ons.

Edit only after **native confirmed STOP**; apply validated JSON through
an off-thread candidate `loadGraph`, await ACK, then issue fresh START.
Refusal retains the draft and must never start it. Applying may reset
unrelated modules. Studio UI, completed-STOP/playhead reporting, native
platform evidence and owner listening remain separate release gates.
