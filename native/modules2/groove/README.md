<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove playable v1 — native G1 config v2 (#496)

`sequencing.groove` is a two-track sixteenth-note sequencer with independent
8/16/32/64-step lengths. **Persisted patch JSON**, not the native transient
POD, defines its behavior. Existing authored `config.version:1` patches keep
their exact S1(a) 32-step clocks and full-length gates; they are accepted by
the v2 engine without rewriting the saved document. G1 is not the Studio
G2 editor and not a hardware listening acceptance.

New patterns use `{"version":2,"bpm":120,"tracks":[{"length":16,
"steps":[{"kind":"note","key":60,"velocity":0.8,"gate":0.5,
"cond":{"type":"chance","p":50},"mod1":0.25,"mod2":0.75},
{"kind":"tie","gate":0.75}]}]}`. V2 lengths are one of 8/16/32/64,
default 16 if omitted; v1 missing lengths imply 32. Omitted steps are rests.
At most two tracks play, with the longest active length setting the master
cycle for global STOP; track STOP uses its own loop end, track START the next
16-step bar. Chance uses a reproducible instance/track-seeded xorshift32.
Other `cond` types: `{"type":"ab","a":1,"b":4}` (track loops),
`{"type":"first"}`, and `{"type":"not_first"}`; no condition means
always. Conditions are accepted only on note steps. A skipped trig drops its
note/ties but still updates its Mod1/Mod2. Gate length is 1/4, 1/2, 3/4
or 1 step; ties extend through to the last tie step's gate. Zero velocity
remains a rest. Unsupported keys (including future chord `keys`) and invalid
ranges refuse before any graph mutation.

S0 output slots remain `t1_pitch` (V/oct, C4=0), `t1_gate` (0/1),
`t1_mod1`, `t1_mod2` (sample-and-hold 0..1), `t1_notes` (matched
NOTE_ON/OFF). Five new slots append `t2_pitch/gate/mod1/mod2/notes`.
Both mods latch on admitted NOTE_ON; rest/tie/stop/reset hold pitch/mods.
A fresh load is always stopped, including saved `run=1`; only a fresh
low→high edge starts step 1. Pending NOTE_OFF blocks new NOTE_ON.

Scalar IDs: `run` 0, `stop` 1, `reset` 2, `mute` 3,
`t1_run/stop/mute` 4/5/6, `t2_run/stop/mute` 7/8/9.
All are rising-edge commands after first-sample baseline. Global STOP queues to the longest active 8/16/32/64-step master cycle;
track STOP queues to its own length, and track START to the next 16-step
bar boundary. START arms/queues;
MUTE releases immediately without losing phase; RESET releases and
clears queues. Priority RESET > STOP > MUTE > START > step advance.
All same-frame releases occur before any new NOTE_ON.

The patch stores structured JSON, never the internal C++ byte layout.
The runtime adapter parses off the render thread. Stopping this module
does **not** authorize mutating a graph whose render callback is active.
The G3 native authoritative STOP/playhead observation is appended as
ports 10–17 and routed through the current-generation telemetry snapshot;
the G2 editor is separate application work. No live-edit, seamless Apply or physical listening is claimed.

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

Native transient POD version 2 reserves 4×64 slots (only two tracks active).
Version-1 transient layout is accepted and upgraded with length32/full gate;
module-state version 2 saves loop/RNG counters and version-1 state defaults them
to loop 1 and the deterministic seed. The patch format remains JSON, and a
v1-authored patch does not gain v2-only fields on load.
Live per-step DSP editing remains deferred to S2.
