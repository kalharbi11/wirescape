<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove native G1/G4a/G5a design (after S1/G3)

Original C++17, no imported third-party code; Westlicht Performer is only
a behavioral reference. Keel contract: #432 comment 6731562789.
Persist original authored JSON `version:1` losslessly and accept explicit
`version:2` with length/gate/cond. Decode legacy transient v1 into the
validated fixed 4×64 v2 POD, never persist POD padding. Off-thread graph
instance IDs become deterministic seed material, not new authored JSON.
Two tracks follow independent 8/16/32/64-step clocks at `div` 1/2/4/8
sixteenths per step, realigning on the maximum armed `length × div`
master cycle (up to 512 ticks). The absolute sixteenth counter wraps at
512. Every track owns five fixed sounding and pending voice slots. The
transient `authored_version` distinction protects v1's original 32-step
track-entry behavior; v2 queues track entries at 16-step bar boundaries. Failed NOTE_OFF retries before new ON. Conditional skipped-note
trigs suppress NOTE_ON and dependent ties while updating the step's mods.
Short gates release at a fractional slowed step, with lookahead over
bounded ties; full-length legacy gates keep exact old boundary-release
timing. On note steps, the strict off-render JSON adapter accepts optional
root-first sorted `keys` (1–5 integer MIDI keys), and defaults to a single
root. The five notes attack at one sample offset in array order; each
accepted voice has a distinct live/pending ID. A partial sink refusal keeps
only accepted notes; refused releases stay owed, and all owed OFFs are
attempted before any new ON. Divisors scale note gate, tie and track-loop
timing without changing the one-key/div-1 golden output. Initial high is baseline. RESET > STOP > MUTE > START > tick.
Render/reset avoid allocation, locks, JSON, logging and I/O.
Observation ports append after the ten published outputs (no ABI edit):
`t1_pos`/`t2_pos` (slots 10/11, CV_BI) report the current 0-based step,
or -1 only when the global transport, track activity, gate and all
owed NOTE_OFFs have stopped. A globally running but disarmed track is not
a completed STOP. This preserves G3's release-safe Apply contract. A queued STOP, mute or refused release never proves
completion. `t1_loop_lo`/`t2_loop_lo` (12/13) and
`t1_loop_hi`/`t2_loop_hi` (16/17) carry the exact low/high 16-bit halves
of the uint32 loop counter; non-render readers reconstruct `lo | (hi << 16)`.
`t1_fired`/`t2_fired` (14/15, gate) report whether the current note trig
passed its condition, inherited through continued ties, false on rests/STOP.
A zero-velocity note is a rest for `fired`; passed note trigs
carry their fired state through ties. Loop starts at 1 on Start/Reset and
advances on each *track's* own wrap. Chance is xorshift32 per track, seeded
from the transient graph instance ID and optional engine seed; RNG and loop
counters save in module-state envelope version 2 (older v1 state defaults
to loop 1). All observation buffers are ordinary channel-0 signals.
Sidecar and WASM install uncabled taps and consume one render-owned last-frame
snapshot. JSON creation stays on the non-render reader. See ENGINE_PROTOCOL §7
and #497 for the additive maps and current-generation stopped Apply gate.
