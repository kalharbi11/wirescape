<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1 native module

`sequencing.groove` is a native, two-track, 32-sixteenth-note sequencer. This is
the UI-independent S1(a) core, not the stopped Studio editor or release sign-off.

The authored patch stores JSON `modules[].config`, not C++ struct bytes. The
version 1 schema is backward-compatible with the S0 one-track format:

```json
{
  "version": 1,
  "bpm": 120,
  "tracks": [
    {"armed": true, "mute": false,
     "steps": [
       {"kind":"note","key":60,"velocity":0.8,"mod1":0.25,"mod2":0.75},
       {"kind":"tie"},
       {"kind":"rest"}
     ]},
    {"steps":[{"kind":"note","key":67,"velocity":0.9}]}
  ]
}
```

BPM must be 20–300. One or two tracks, each with 1–32 authored steps, are
accepted; omitted steps are rests until the shared 32-step/two-bar wrap.
Steps support `note`, `tie`, or `rest`. Notes require an integer MIDI
`key` in 0..127 and `velocity` in 0..1. Any step may have optional
`mod1` and `mod2` in 0..1 (default zero); optional track `armed`
defaults true and `mute` defaults false. A note with velocity zero is a
rest. Ties sustain a sounding note and a tie without a note is a rest.
Unknown fields, wrong types, unsupported versions and invalid values
are refused before graph activation. Saved v1 S0 JSON is accepted unchanged.

## Ports, units, controls

Track 1 retains the five S0 slots `t1_pitch` (1V/oct, C4=0),
`t1_gate` (0 or 1), `t1_mod1`, `t1_mod2` (sample-held 0..1),
and `t1_notes` (matched NOTE_ON/OFF). Track 2 appends
`t2_pitch`, `t2_gate`, `t2_mod1`, `t2_mod2`, `t2_notes` in
that order. t3/t4 slots are reserved but not declared. Both tracks use
one audio-frame musical clock at 16ths, 32 steps per cycle; multiple
module instances have independent clocks.

Parameter slots: `run`=0 (S0 ID retained), `stop`=1, `reset`=2,
`mute`=3, `t1_run`=4, `t1_stop`=5, `t1_mute`=6,
`t2_run`=7, `t2_stop`=8, `t2_mute`=9.
Every control is an edge request: a **fresh low→high transition**.
At load, the first observed value is only a baseline, including saved
high values. Set the parameter to low and apply a new high to request it
again. A browser event has control-thread latency; the native parameter
observation sample is the scheduling point.

- Global `run` while stopped plays step 1 for armed tracks. While
  playing, it cancels queued global stop without retriggering.
- Global `stop` toggles stop queued for the next shared boundary,
  after step 32 finishes. Stopping releases notes and gates; the
  boundary does not emit an extra step-1 note.
- Track `run` arms while globally stopped, joins at the next shared
  boundary when globally running, or cancels its pending stop.
- Track `stop` disarms while globally stopped, cancels queued entry,
  or toggles the cycle-end stop. Starting an already playing track does
  not reset its phase.
- Global/track `mute` toggles effective mute immediately, releases
  notes/gates, and does not stop the shared clock. Unmute permits
  triggering only on the next eligible note step.
- `reset` releases notes, clears queued actions, resets phase, keeps
  running/armed/mute intent, and (when running) waits one 16th interval
  before step 1. No same-sample reset/retrigger.
- Same-frame priority is RESET > STOP > MUTE > START > scheduler;
  releases occur before any admitted new note. Saturated NOTE_OFF
  events are retried; NOTE_ON is dropped while its prior release
  remains pending.

Pitch and both mods hold their last triggered values on rests, ties,
stop and reset. Gates/events drop on releases. A disabled second track
always outputs zero and never emits notes.

## Apply and release boundary

Only an acknowledged stopped state permits editing the authored draft.
Use a control-side candidate graph load/ACK and then an explicit fresh
START edge, never mutate a live graph merely because Groove is stopped.
Refused configuration retains draft state and never starts; load/restart
always begins stopped. Existing full-graph Apply may reset unrelated
modules. The Studio stopped editor, native completed-STOP/playhead
observation and end-to-end draft/refusal reconciliation are a separate
S1(b) deliverable; this native PR **does not** claim them.

Tests and CI never open audio/MIDI devices. Physical listening belongs to
the owner, and S1 requires exact-head Windows and private Pi evidence.
